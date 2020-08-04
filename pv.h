#ifndef PV_H
#define PV_H

#include <string>
#include <deque>
#include <cadef.h>
#include <alarm.h>
#include <epicsTime.h>
#include <epicsMutex.h>
#include <epicsGuard.h>
#include <pv/sharedVector.h>

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

class Orbit;

struct CAContext {
    static size_t num_instances;

    explicit CAContext(unsigned int prio, bool fake=false);
    ~CAContext();

    struct ca_client_context *context;

    // manage attachment of a context to the current thread
    struct Attach {
        struct ca_client_context *previous;
        Attach(const CAContext&);
        ~Attach();
    };

    EPICS_NOT_COPYABLE(CAContext)
};

struct DBRValue {
    struct Holder {
        static size_t num_instances;
        epicsTimeStamp ts;
        epicsUInt16 sevr;
        epicsUInt16 stat;
        epicsUInt32 count;
        epics::pvData::shared_vector<const void> buffer;
        Holder();
        ~Holder();
    };
private:
    std::shared_ptr<Holder> held;
public:
    DBRValue() {}
    DBRValue(Holder *H) : held(H) {}
    bool valid() const { return !!held; }
    Holder* operator->() {return held.get();}
    const Holder* operator->() const {return held.get();}
    void swap(DBRValue& o) {
        held.swap(o.held);
    }
    void reset() {
        held.reset();
    }
};

struct PV {
public:
    PV(const std::string& pvname, const CAContext& context, size_t limit, Orbit& orbit);
    ~PV();
    static size_t num_instances;
    const std::string pvname;
    const CAContext& context;
    Orbit& orbit;
    bool connected;
    bool ready;
    mutable epicsMutex mutex;
    std::deque<DBRValue> values;
    DBRValue pop();
    size_t limit;
    void close();
    void clear(size_t remain);
private:
    void connect();
    chid chan;
    evid ev;
    epicsTimeStamp last_event;
    void push(DBRValue& v);
    static void connectionCallback(struct connection_handler_args args);
    static void monitorCallback(struct event_handler_args args);
};

#endif //PV_H