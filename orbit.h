#ifndef ORBIT_H
#define ORBIT_H

#include <vector>
#include <array>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <set>
#include <map>
#include <chrono>
#include <epicsTypes.h>
#include <epicsEvent.h>
#include "pv.h"

struct OrbitData {
    epicsTimeStamp ts;
    std::vector<std::array<DBRValue, 3>> values;
    bool complete;
};

struct Receiver {
    virtual ~Receiver() {}
    virtual void setNames(const std::vector<std::string>& n) = 0;
    virtual void setZs(const std::vector<double>& zs) = 0;
    virtual void setCompletedOrbit(const OrbitData& completed_orbit) = 0;
};

class Orbit {
private:
    std::vector<std::array<std::shared_ptr<PV>, 3>> pvs;
    bool run;
    std::mutex mutex;
    epicsEvent wakeup;
    std::vector<std::string> names;
    std::vector<double> zs;
    std::thread processingThread;
    
    bool waiting;
    std::set<Receiver*> receivers;
    bool receivers_changed;
    
    typedef std::map<epicsUInt64, OrbitData> events_t;
    events_t events;
    std::set<Receiver*> receivers_shadow;
    epicsTimeStamp now;
    epicsUInt64 now_key, oldest_key;
    bool hasCompleteOrbit;
    OrbitData latestCompleteOrbit;
    std::vector<OrbitData> completed;
    void process();
    void dequeue_pv_data();
    void check_for_complete();
public:
    Orbit(CAContext& context, const std::vector<std::string>& bpm_names, const std::vector<double>& z_vals, const std::string& edef_suffix);
    ~Orbit();
    bool connected();
    void wake();
    void close();
    bool wait_for_connection(std::chrono::seconds timeout);
    void add_receiver(Receiver *);
    void remove_receiver(Receiver *);
};

#endif //ORBIT_H
