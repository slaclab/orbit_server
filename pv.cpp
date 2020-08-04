#include "pv.h"
#include "orbit.h"
#include <db_access.h>
#include <stdexcept>
#include <epicsThread.h>
#include <cadef.h>
#include <pv/reftrack.h>
#include <errlog.h>

namespace pvd = epics::pvData;

namespace {
    struct eca_error : public std::runtime_error
    {
        explicit eca_error(int code, const char *msg =0) :std::runtime_error(buildMsg(code)) {}

        static std::string buildMsg(int code, const char *msg =0) {
            std::ostringstream strm;
            strm<<(msg?msg:"CA Error")<<" : "<<ca_message(code);
            return strm.str();
        }

        static void check(int code, const char *msg =0) {
            if(code!=ECA_NORMAL)
                throw eca_error(code, msg);
        }
    };
}

void onError(exception_handler_args args)
{
    errlogPrintf("Collector CA exception on %s : %s on %s:%u\n%s",
                 args.chid?ca_name(args.chid):"<unknown>",
                 ca_message(args.stat),
                 args.pFile, args.lineNo, args.ctx);
}

size_t DBRValue::Holder::num_instances;

DBRValue::Holder::Holder() : sevr(4), stat(LINK_ALARM), count(1u) {
    REFTRACE_INCREMENT(num_instances);
    ts.secPastEpoch = 0;
    ts.nsec = 0;
}

DBRValue::Holder::~Holder() {
    REFTRACE_DECREMENT(num_instances);
}

size_t CAContext::num_instances;

CAContext::CAContext(unsigned int prio, bool fake)
    :context(0)
{
    REFTRACE_INCREMENT(num_instances);
    if(fake) {
        return;
    };

    epicsThreadId me = epicsThreadGetIdSelf();
    unsigned int orig_prio = epicsThreadGetPrioritySelf();

    // the CA context we create will inherit our priority
    epicsThreadSetPriority(me, prio);

    struct ca_client_context *current = ca_current_context();
    if(current) {
        ca_detach_context();
    }
    try {
        int err = ca_context_create(ca_enable_preemptive_callback);
        eca_error::check(err, "Create context");

        context = ca_current_context();
        if(!context) {
            throw std::logic_error("Create context, but not really?");
        }
       
        err = ca_add_exception_event(&onError, 0);
        eca_error::check(err, "Change CA exception handler");

        ca_detach_context();

    }catch(...){
        if(ca_current_context()) {
            ca_detach_context();
        }
            
        if(current) {
            ca_attach_context(current);
        }
        epicsThreadSetPriority(me, orig_prio);
        throw;
    }
    if(current) {
        ca_attach_context(current);
    }
    epicsThreadSetPriority(me, orig_prio);
}

CAContext::~CAContext()
{
    REFTRACE_DECREMENT(num_instances);

    if(!context) {
        return;
    }

    struct ca_client_context *current = ca_current_context();
    if(current) {
        ca_detach_context();
    }

    ca_attach_context(context);
    ca_context_destroy();

    if(current){
        ca_attach_context(current);
    }
}

CAContext::Attach::Attach(const CAContext &ctxt)
    :previous(ca_current_context())
{
    if(previous){
        ca_detach_context();
    }

    ca_attach_context(ctxt.context);
}

CAContext::Attach::~Attach()
{
    ca_detach_context();

    if(previous){
        ca_attach_context(previous);
    }
}

size_t PV::num_instances;

PV::PV(const std::string& pvname, const CAContext& context, size_t limit, Orbit& orbit) : 
    pvname(pvname),
    context(context),
    orbit(orbit),
    connected(false),
    ready(false),
    limit(limit),
    chan(0),
    ev(0)
{
    REFTRACE_INCREMENT(num_instances);
    last_event.secPastEpoch = 0;
    last_event.nsec = 0;
    if(!context.context){
        return;
    }
    CAContext::Attach A(context);
    
    int err = ca_create_channel(pvname.c_str(), &connectionCallback, this, 0, &chan);
    eca_error::check(err, "Create Channel");
}

PV::~PV() {
    printf("Clearing PV: %s\n", pvname.c_str());
    close();
    REFTRACE_DECREMENT(num_instances);
}

void PV::close() {
    if(!context.context){
        return;
    }
    {
        Guard G(mutex);
        if(!chan) {
            return;
        }
    }
    CAContext::Attach A(context);
    int err = ca_clear_channel(chan);
    {
        Guard G(mutex);
        chan = 0;
        ev = 0;
    }
    eca_error::check(err);
}

void PV::clear(size_t remain) {
    Guard G(mutex);
    while(values.size() > remain) {
        values.pop_front();
    }
}

DBRValue PV::pop() {
    DBRValue ret;
    {
        Guard G(mutex);
        if(!values.empty()) {
            ret = values.front();
            values.pop_front();
        }
    }
    return ret;
}

void PV::push(DBRValue& v) {
    while(values.size() > limit) {
        values.pop_front();
    }
    values.push_back(DBRValue());
    values.back().swap(v);
}

void PV::connectionCallback(connection_handler_args args) {
    PV *self = static_cast<PV*>(ca_puser(args.chid));
    try {
        if (args.op == CA_OP_CONN_UP) {
            short native = ca_field_type(args.chid);
            short promoted = dbf_type_to_DBR_TIME(native);
            //unsigned long maxcnt = ca_element_count(args.chid);
            if(native == DBF_STRING) {
                return;
            }
            const int err = ca_create_subscription(promoted, 0, args.chid, DBE_VALUE|DBE_ALARM, &monitorCallback, self, &self->ev);
            eca_error::check(err);
            {
                Guard G(self->mutex);
                self->last_event.secPastEpoch = 0;
                self->last_event.nsec = 0;
                self->connected = true;
                self->limit = size_t(4u);
            }
        } else if(args.op == CA_OP_CONN_DOWN) {
            if(!self->ev) {
                return;
            }
            const int err = ca_clear_subscription(self->ev);
            self->ev = 0;
        
            DBRValue val(new DBRValue::Holder);
            epicsTimeGetCurrent(&val->ts);
            bool notify = false;
            {
                Guard G(self->mutex);
                notify = self->values.empty();
                self->connected = false;
                self->push(val);
            }
            if(notify) {
                self->ready = true;
                self->orbit.wake();
            }
            eca_error::check(err);
        }
    } catch(std::exception& err) {
        printf("Exception in PV::connectionSubscription() for %s: %s\n", ca_name(args.chid), err.what());
    }
}

void PV::monitorCallback(event_handler_args args) {
    PV *self = static_cast<PV*>(args.usr);
    try {
        if(!dbr_type_is_TIME(args.type)) {
            throw std::runtime_error("CA server doesn't honor DBR_TIME_*");
        }
        size_t count = args.count;
        size_t elem_size = dbr_value_size[args.type];
        size_t size = dbr_size_n(args.type, args.count);
        if(args.count == 0 && size > elem_size) {
            size -= elem_size;
        }
        pvd::ScalarType type;
        switch(args.type) {
            case DBR_TIME_STRING: type = pvd::pvString; break;
            case DBR_TIME_SHORT: type = pvd::pvShort; break;
            case DBR_TIME_FLOAT: type = pvd::pvFloat; break;
            case DBR_TIME_ENUM: type = pvd::pvShort; break;
            case DBR_TIME_CHAR: type = pvd::pvByte; break;
            case DBR_TIME_LONG: type = pvd::pvInt; break;
            case DBR_TIME_DOUBLE: type = pvd::pvDouble; break;
            default:
                type = pvd::pvByte; break;
        }
        dbr_time_double meta;
        memcpy(&meta, args.dbr, offsetof(dbr_time_double, value));
        pvd::shared_vector<void> buf;
        if(type != pvd::pvString) {
            buf = pvd::ScalarTypeFunc::allocArray(type, count);
            if(buf.size() != elem_size * count) {
                throw std::logic_error("DBR buffer size computation error");
            }
            memcpy(buf.data(), dbr_value_ptr(args.dbr, args.type), buf.size());
        } else {
            printf("%s DBF_STRING not supported, ignoring\n", self->pvname.c_str());
            return;
        }
        
        DBRValue val(new DBRValue::Holder);
        val->sevr = meta.severity;
        val->stat = meta.status;
        val->ts = meta.stamp;
        val->count = count;
        val->buffer = pvd::freeze(buf);
        assert(val->buffer.data() != nullptr);
        bool notify = false;
        {
            Guard G(self->mutex);
            if(epicsTimeDiffInSeconds(&meta.stamp, &self->last_event) > 0) {
                notify = self->values.empty();
                self->push(val);
            }
            self->last_event = meta.stamp;
        }
        if(notify) {
            self->ready = true;
            self->orbit.wake();
        }
    } catch(std::exception& err) {
        printf("Unexpected exception in PV::monitorCallback() for %s: %s\n", ca_name(args.chid), err.what());
    }
}