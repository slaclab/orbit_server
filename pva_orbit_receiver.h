#ifndef PVA_ORBIT_RECEIVER_H
#define PVA_ORBIT_RECEIVER_H

#include <string>
#include <vector>
#include <memory>
#include <pvxs/server.h>
#include <pvxs/sharedpv.h>
#include <pvxs/sharedArray.h>
#include <pvxs/nt.h>
#include "orbit.h"

struct PVAOrbitReceiver : public Receiver
{
    static size_t num_instances;
    PVAOrbitReceiver(Orbit& orbit);
    virtual ~PVAOrbitReceiver();
    Orbit& orbit;
    std::shared_ptr<pvxs::server::SharedPV> pv;
    epicsMutex mutex;
    pvxs::Value orbitValue;
    void close();
    virtual void setNames(const std::vector<std::string>& n);
    virtual void setZs(const std::vector<double>& zs);
    virtual void setCompletedOrbit(const OrbitData& o);
private:
    pvxs::shared_array<std::string> _names;
    pvxs::shared_array<double> _zs;
    bool initialized;
    OrbitData publishedOrbit;
};


#endif // PVA_ORBIT_RECEIVER_H