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
//static double maxEventRate = 20;
// timeout to flush partial events
static double maxEventAge = 1.0;
// holdoff after delivering events (in milliseconds).  16 ms is about 60 Hz.
int flushPeriod = 4;

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
        const std::lock_guard<std::mutex> lock(mutex);
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
    const std::lock_guard<std::mutex> lock(mutex);
    receivers.erase(recv);
    receivers_changed = true;
}

void Orbit::process() {
    epicsTimeGetCurrent(&now);
    while(run) {
        {
            const std::lock_guard<std::mutex> lock(mutex);
            waiting = false;
            now_key = now.secPastEpoch;
            now_key <<= 32;
            now_key |= now.nsec;
            dequeue_pv_data();
            check_for_complete();

            if (receivers_changed) {
                receivers_shadow = receivers;
                receivers_changed = false;
            }
        }
        if(!completed.empty()) {
            {
                for (std::set<Receiver*>::iterator it(receivers_shadow.begin()), end(receivers_shadow.end()); it != end; ++it) {
                    (*it)->setCompletedOrbit(completed.back());
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(flushPeriod));
            }
            const std::lock_guard<std::mutex> lock(mutex);
            completed.clear();
            hasCompleteOrbit = false;
        }
        const std::lock_guard<std::mutex> lock(mutex);
        if (waiting) {
            wakeup.wait();
        }
        epicsTimeGetCurrent(&now);
    }
}

void Orbit::dequeue_pv_data() {
    bool all_queues_empty = true;
    for (size_t i=0, N=pvs.size(); i<N; i++) {
        for (size_t j=0; j<3; j++) {
            auto pv = pvs[i][j];
            DBRValue val(pv->pop());
            if (!val.valid()) {
                continue;
            }
            all_queues_empty = false;
            epicsUInt64 key = ((epicsUInt64)(val->ts.secPastEpoch)) << 32 | val->ts.nsec;
            if (key <= oldest_key) {
                continue;
            }
            bool orbitExists = false;
            if (events.find(key) != events.end()) {
                orbitExists = true;
            }
            events_t::mapped_type& incompleteOrbit = events[key];
            if (!orbitExists) {
                //This orbit is brand new, so we need to initialize it.
                incompleteOrbit.values.resize(pvs.size());
                incompleteOrbit.complete = false;
                incompleteOrbit.ts = val->ts;
            }
            if (incompleteOrbit.complete) {
                printf("Something wen't wrong - found a new value for an already-complete orbit.\n");
            }
            if (!incompleteOrbit.values[i][j].valid()) {
                incompleteOrbit.values[i][j].swap(val);
            } else {
                printf("Uh oh, recieved a duplicate value with same timestamp.\n");
            }
        }
    }
    waiting = all_queues_empty;
}

void Orbit::check_for_complete() {
    epicsUInt64 max_age = maxEventAge;
    max_age <<= 32;
    max_age |= epicsUInt32(1000000000u * fmod(maxEventAge, 1.0));
    events_t::iterator first_incomplete(events.end());
    size_t i = events.size();
    size_t numComplete = 0;
    {
        //First, erase all incomplete orbits that are too old.
        for(auto it = events.begin(); it != events.end();) {
            epicsInt64 key_age = epicsInt64(now_key) - epicsInt64(it->first);
            if(key_age >= epicsInt64(max_age)) {
                events.erase(it++);
            } else {
                ++it;
            }
        }

        events_t::reverse_iterator it(events.rbegin()), end(events.rend());
        //We're iterating from newest to oldest here.
        for(; it != end; ++it, i--) {
            events_t::mapped_type& orbit = it->second;
            bool complete = true;
            for (size_t i=0, N=pvs.size(); complete && i<N; i++) {
                for (size_t j=0; j<3; j++) {
                    complete = complete && (!pvs[i][j]->connected || orbit.values[i][j].valid());
                }
            }
            if (complete) {
                orbit.complete = true;
                numComplete++;
            }
        }
    }

    completed.clear();
    if (numComplete == 0) {
        return;
    }
    events_t::iterator it(events.begin()), end(events.end());
    ///Now we iterate from oldest to newest.
    completed.reserve(numComplete);
    while(it != end) {
        events_t::iterator cur = it++;
        if (cur->second.complete) {
            oldest_key = cur->first;
            completed.push_back(cur->second);
            events.erase(cur);
        }
    }

    while(events.size() > 10) {
        events.erase(events.begin());
    }
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