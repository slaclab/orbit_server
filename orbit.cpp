#include <stdio.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <thread>
#include <cmath>
#include "orbit.h"

// limit on number of potentially complete events to track
static double maxEventRate = 20;
// timeout to flush partial events
static double maxEventAge = 1.0;
// holdoff after delivering events (in milliseconds).  16 ms is about 30 Hz.
int flushPeriod = 16;

Orbit::Orbit(CAContext& context, const std::vector<std::string>& bpm_names, const std::vector<double>& z_vals, const std::string& edef_suffix) : 
//context(context),
run(true),
names(bpm_names),
zs(z_vals),
waiting(false),
oldest_key(0u),
hasCompleteOrbit(false)
{
    printf("Making orbit from vector...\n");
    std::string axes[3] = {"X", "Y", "TMIT"};
    pvs.resize(bpm_names.size());
    for(size_t i=0, N=bpm_names.size(); i<N; i++) {
        for (size_t j=0; j<3; j++) {
            pvs[i][j].reset(new PV(bpm_names[i] + ":" + axes[j] + edef_suffix, context, 10u, *this));
        }
    }
    
    processingThread = std::thread(&Orbit::process, this);   
}

Orbit::~Orbit() {
    close();
}

void Orbit::close() {
    for(size_t i=0, N=pvs.size(); i<N; i++) {
        for (size_t j=0; j<3; j++) {
            pvs[i][j]->close();
        }
    }
    
    wakeup.signal();
    processingThread.join();
}

void Orbit::wake() {
    if (waiting) {
        //printf("Waking up!");
        wakeup.signal();
    }
}

bool Orbit::connected() {
    bool conn = true;
    for(size_t i=0, N=pvs.size(); i<N; i++) {
        for(size_t j=0; j<3; j++) {
            conn = conn && pvs[i][j]->connected;
        }
    }
    return conn;
}

void Orbit::add_receiver(Receiver* recv) {
    std::vector<std::string> recv_names;
    std::vector<double> recv_zs;
    {
        Guard G(mutex);
        receivers.insert(recv);
        receivers_changed = true;
        recv_names.reserve(names.size());
        recv_zs.reserve(zs.size());
        for (size_t i=0, N=names.size(); i<N; i++) {
            recv_names.push_back(names[i]);
            recv_zs.push_back(zs[i]);
        }
    }
    recv->setNames(recv_names);
    recv->setZs(recv_zs);
}

void Orbit::remove_receiver(Receiver* recv) {
    Guard G(mutex);
    receivers.erase(recv);
    receivers_changed = true;
}

void Orbit::process() {
    Guard G(mutex);
    epicsTimeGetCurrent(&now);
    while(run) {
        waiting = false;
        now_key = now.secPastEpoch;
        now_key <<= 32;
        now_key |= now.nsec;
        process_dequeue();
        process_test();
        
        if (receivers_changed) {
            receivers_shadow = receivers;
            receivers_changed = false;
        }
        
        bool willwait = waiting;
        if(hasCompleteOrbit) {
            //printf("Huzzah! A complete orbit!\n");
            {
                UnGuard U(G);
                for (std::set<Receiver*>::iterator it(receivers_shadow.begin()), end(receivers_shadow.end()); it != end; ++it) {
                    (*it)->setCompletedOrbit(latestCompleteOrbit);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(flushPeriod));
            }
            hasCompleteOrbit = false;
        }
        if (willwait) {
            //printf("%u - Nothing to do.  Sleeping!\n", now.secPastEpoch);
            wakeup.wait();
        }
        epicsTimeGetCurrent(&now);
    }
}

void Orbit::process_dequeue() {
    bool nothing = false;
    unsigned maxEvents = std::max(10.0, std::min(maxEventRate*flushPeriod, 1000.0));
    while(!nothing && events.size() < maxEvents) {
        nothing = true;
        for (size_t i=0, N=pvs.size(); i<N; i++) {
            for (size_t j=0; j<3; j++) {
                auto pv = pvs[i][j];
                if((i != 0 && !pv->ready) || !pv->connected) {
                    //printf("%s is not ready yet.  %s\n", pv->pvname.c_str(), pv->connected?"connected":"disconnected");
                    continue;
                }
                DBRValue val(pv->pop());
                if(!val.valid()) {
                    //printf("%s is not valid yet.\n", pv->pvname.c_str());
                    pv->ready = false;
                    continue;
                }
                pv->ready = true;
                nothing = false;
                epicsUInt64 key = val->ts.secPastEpoch;
                key <<= 32;
                key |= val->ts.nsec;
                //printf("%s is valid and ready! %llu.\n", pv->pvname.c_str(), key);
                if (!pv->connected || key > oldest_key) {
                    events_t::mapped_type& incompleteOrbit = events[key]; //This implicitly allocates new incompleteOrbits
                    incompleteOrbit.complete = false;
                    incompleteOrbit.ts = val->ts;
                    incompleteOrbit.values.resize(pvs.size());
                    if(!incompleteOrbit.values[i][j].valid()) {
                        incompleteOrbit.values[i][j].swap(val);
                    }
                }
            }
        }
    }

    if (!nothing) {
        //If you get here, this means the event buffer overflowed.
        //We couldn't keep up with incoming events!
        printf("Uh oh, overflow!\n");
        for(size_t i=0, N=pvs.size(); i<N; i++) {
            for(size_t j=0; j<3; j++) {
                auto pv = pvs[i][j];
                if(pv){
                    pv->clear(4);
                }
            }
        }
    }
    waiting = nothing;
}

void Orbit::process_test() {
    //printf("BEGIN process_test()\n\n");
    epicsUInt64 max_age = maxEventAge;
    max_age <<= 32;
    max_age |= epicsUInt32(1000000000u * fmod(maxEventAge, 1.0));
    events_t::iterator first_partial(events.end());
    size_t i = events.size();
    {
        //printf("Starting completion checks.\n");
        events_t::reverse_iterator it(events.rbegin()), end(events.rend());
        /* Note for a C++ noob: rbegin points to the "largest" key, which is
        the newest event (largest timestamp), in our case.  So we're iterating 
        from newest to oldest here. */
        for(; it!=end; ++it, i--) {
            //printf("Checking event: %u, %u...", it->second.ts.secPastEpoch, it->second.ts.nsec);
            epicsInt64 key_age = epicsInt64(now_key) - epicsInt64(it->first);
            if(key_age >= epicsInt64(max_age)) {
                //This event, and all others after it, are older than max_age.
                //Don't even bother checking for completion.
                //printf("\n Older than max_age!  Stopping completion checks.\n\n");
                break;
            }
            const events_t::mapped_type& orbit = it->second;
            bool complete = true;
            for (size_t j=0, N=pvs.size(); complete && j<N; j++) {
                for (size_t k=0; k<3; k++) {
                    complete = !(pvs[j][k]->connected) || orbit.values[j][k].valid();
                    /*
                    if(!complete) {
                        printf("## test slice %llx found incomplete %s %sconn %svalid\n",
                                     it->first, pvs[j][k]->pvname.c_str(),
                                     !pvs[j][k]->connected?"dis":"",
                                     orbit.values[j][k].valid()?"":"in");
                    }
                    */
                }
            }
            it->second.complete = complete;
            if (!complete) {
                //printf("incomplete.\n");
            } else {
                //printf("complete!\n");
                if (!hasCompleteOrbit) {
                    if (it->first > oldest_key) {
                        //printf("Setting latestCompleteEvent: %u, %u\n", it->second.ts.secPastEpoch, it->second.ts.nsec);
                        latestCompleteOrbit = it->second;
                        hasCompleteOrbit = true;
                        oldest_key = it->first;
                    } else {
                        //printf("Newly complete orbit that was older than the latest published: %u, %u\n", it->second.ts.secPastEpoch, it->second.ts.nsec);
                    }   
                }
            }
        }
    }
    if(false) {
        if(first_partial==events.begin() || events.empty()) {
            printf("## No events complete\n");
        } else {
            printf("## %zu events complete out of %zu events total\n", events.size()-i, events.size());
        }
    }
    events_t::iterator it(events.begin()), end(events.end());
    while (it != end) {
        epicsInt64 key_age = epicsInt64(now_key) - epicsInt64(it->first);
        if (it->second.complete) {
            //printf("Erasing non-fresh complete orbit: %u, %u\n", it->second.ts.secPastEpoch, it->second.ts.nsec);
            it = events.erase(it);
        } else if (key_age >= epicsInt64(max_age)) {
            //This event, and all others after it, are older than max_age.
            //Don't even bother checking for completion.
            //printf("Erasing orbit older than max_age: %u, %u\n", it->second.ts.secPastEpoch, it->second.ts.nsec);
            it = events.erase(it);
        } else {
            ++it;
        }
        
    }
    while (events.size() > 4) {
        //printf("Erasing old incomplete orbit: %u, %u\n", events.begin()->second.ts.secPastEpoch, events.begin()->second.ts.nsec);
        events.erase(events.begin());
    }
    //printf("END process_test()\n\n");
}

bool Orbit::wait_for_connection(std::chrono::seconds timeout) {
  auto start_time = std::chrono::steady_clock::now();
  while (connected() == false) {
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
    if (elapsed_seconds.count() > std::chrono::milliseconds(timeout).count()) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return true;
}