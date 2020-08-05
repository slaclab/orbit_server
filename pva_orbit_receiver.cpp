#include "pva_orbit_receiver.h"
#include <pv/sharedVector.h>
#include <pvxs/data.h>
#include <db_access.h>


PVAOrbitReceiver::PVAOrbitReceiver(Orbit& orbit) :
orbit(orbit)
{
    pv = std::make_shared<pvxs::server::SharedPV>(pvxs::server::SharedPV::buildReadonly());
    auto time_t = {
        pvxs::members::Int64("secondsPastEpoch"),
        pvxs::members::Int32("nanoseconds"),
        pvxs::members::Int32("userTag"),
    };
    auto alarm_t = {
        pvxs::members::Int32("severity"),
        pvxs::members::Int32("status"),
        pvxs::members::String("message"),
    };
    
    orbitValue = pvxs::TypeDef(pvxs::TypeCode::Struct, "OrbitTable", {
        pvxs::members::StringA("labels"),
        pvxs::members::Struct("value", {
            pvxs::members::StringA("device_name"),
            pvxs::members::Float64A("z"),
            pvxs::members::Float64A("x_val"),
            pvxs::members::Int32A("x_severity"),
            pvxs::members::Int32A("x_status"),
            pvxs::members::Int64A("x_ts_seconds"),
            pvxs::members::Int32A("x_ts_nanos"),
            pvxs::members::Float64A("y_val"),
            pvxs::members::Int32A("y_severity"),
            pvxs::members::Int32A("y_status"),
            pvxs::members::Int64A("y_ts_seconds"),
            pvxs::members::Int32A("y_ts_nanos"),
            pvxs::members::Float64A("tmit_val"),
            pvxs::members::Int32A("tmit_severity"),
            pvxs::members::Int32A("tmit_status"),
            pvxs::members::Int64A("tmit_ts_seconds"),
            pvxs::members::Int32A("tmit_ts_nanos"),
        }),
        pvxs::members::String("descriptor"),
        pvxs::members::Struct("alarm", "alarm_t", alarm_t),
        pvxs::members::Struct("timeStamp", "time_t", time_t),
    }).create();
    pvxs::shared_array<std::string> labels({"Device Name", "Z", "X", "Y", "TMIT"});
    orbitValue["labels"] = labels.freeze();
    orbitValue["descriptor"] = "LCLS Orbit Data";
    orbit.add_receiver(this);
}

PVAOrbitReceiver::~PVAOrbitReceiver() {
    close();
}

void PVAOrbitReceiver::close() {
    orbit.remove_receiver(this);
    pv->close();
}

void PVAOrbitReceiver::setNames(const std::vector<std::string>& names) {
    pvxs::shared_array<std::string> ns(names.size());
    for(size_t i=0, N=names.size(); i<N; i++) {
        ns[i] = names[i];
    }
    orbitValue["value.device_name"] = ns.freeze();
}

void PVAOrbitReceiver::setZs(const std::vector<double>& zs) {
    pvxs::shared_array<double> z(zs.size());
    for(size_t i=0, N=zs.size(); i<N; i++) {
        z[i] = zs[i];
    }
    orbitValue["value.z"] = z.freeze();
}

void PVAOrbitReceiver::setCompletedOrbit(const OrbitData& o) {
    //printf("Setting a completed orbit!\n");
    Guard G(mutex);
    pvxs::shared_array<double> x_val(o.values.size());
    pvxs::shared_array<int> x_severity(o.values.size());
    pvxs::shared_array<int> x_status(o.values.size());
    pvxs::shared_array<int64_t> x_ts_seconds(o.values.size());
    pvxs::shared_array<int> x_ts_nanos(o.values.size());
    pvxs::shared_array<double> y_val(o.values.size());
    pvxs::shared_array<int> y_severity(o.values.size());
    pvxs::shared_array<int> y_status(o.values.size());
    pvxs::shared_array<int64_t> y_ts_seconds(o.values.size());
    pvxs::shared_array<int> y_ts_nanos(o.values.size());
    pvxs::shared_array<double> tmit_val(o.values.size());
    pvxs::shared_array<int> tmit_severity(o.values.size());
    pvxs::shared_array<int> tmit_status(o.values.size());
    pvxs::shared_array<int64_t> tmit_ts_seconds(o.values.size());
    pvxs::shared_array<int> tmit_ts_nanos(o.values.size());
    
    for (size_t i=0, N=o.values.size(); i<N; i++) {
        DBRValue xval = o.values[i][0];
        if (xval.valid()) {
            assert(xval->count == 1);
            const epics::pvData::shared_vector<const double>& xval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(xval->buffer));
            x_val[i] = xval_float_buffer[0];
            x_severity[i] = xval->sevr;
            x_status[i]= xval->stat;
            x_ts_seconds[i] = xval->ts.secPastEpoch;
            x_ts_nanos[i] = xval->ts.nsec;
        } else {
            x_val[i] = 0.0;
            x_severity[i] = 3;
        }
        
        DBRValue yval = o.values[i][1];
        if (yval.valid()) {
            assert(yval->count == 1);
            const epics::pvData::shared_vector<const double>& yval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(yval->buffer));
            y_val[i] = yval_float_buffer[0];
            y_severity[i] = yval->sevr;
            y_status[i]= yval->stat;
            y_ts_seconds[i] = yval->ts.secPastEpoch;
            y_ts_nanos[i] = yval->ts.nsec;
        } else {
            y_val[i] = 0.0;
            y_severity[i] = 3;
        }
        
        DBRValue tmitval = o.values[i][2];
        if (tmitval.valid()) {
            assert(tmitval->count == 1);
            const epics::pvData::shared_vector<const double>& tmitval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(tmitval->buffer));
            tmit_val[i] = tmitval_float_buffer[0];
            tmit_severity[i] = tmitval->sevr;
            tmit_status[i]= tmitval->stat;
            tmit_ts_seconds[i] = tmitval->ts.secPastEpoch;
            tmit_ts_nanos[i] = tmitval->ts.nsec;
        } else {
            tmit_val[i] = 0.0;
            tmit_severity[i] = 3;
        }
        
    }
    orbitValue["value.x_val"] = x_val.freeze();
    orbitValue["value.x_severity"] = x_severity.freeze();
    orbitValue["value.x_status"] = x_status.freeze();
    orbitValue["value.x_ts_seconds"] = x_ts_seconds.freeze();
    orbitValue["value.x_ts_nanos"] = x_ts_nanos.freeze();
    
    orbitValue["value.y_val"] = y_val.freeze();
    orbitValue["value.y_severity"] = y_severity.freeze();
    orbitValue["value.y_status"] = y_status.freeze();
    orbitValue["value.y_ts_seconds"] = y_ts_seconds.freeze();
    orbitValue["value.y_ts_nanos"] = y_ts_nanos.freeze();
    
    orbitValue["value.tmit_val"] = tmit_val.freeze();
    orbitValue["value.tmit_severity"] = tmit_severity.freeze();
    orbitValue["value.tmit_status"] = tmit_status.freeze();
    orbitValue["value.tmit_ts_seconds"] = tmit_ts_seconds.freeze();
    orbitValue["value.tmit_ts_nanos"] = tmit_ts_nanos.freeze();
    
    orbitValue["timeStamp.secondsPastEpoch"] = o.ts.secPastEpoch;
    orbitValue["timeStamp.nanoseconds"] = o.ts.nsec;
    auto newOrbitValue = orbitValue.clone();
    {
        //UnGuard U(G);
        if (!pv->isOpen()) {
            pv->open(std::move(newOrbitValue));
        } else {
            pv->post(std::move(newOrbitValue));
        }
    }
    
    orbitValue.unmark(); //Set all fields to unchanged in preparation for the next update.
    
}