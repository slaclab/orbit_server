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
            pvxs::members::StringA("names"),
            pvxs::members::Float32A("z"),
            pvxs::members::StructA("x", {
                pvxs::members::Float32("value"),
                pvxs::members::Struct("alarm", "alarm_t", alarm_t),
                pvxs::members::Struct("timeStamp", "time_t", time_t)
            }),
            pvxs::members::StructA("y", {
                pvxs::members::Float32("value"),
                pvxs::members::Struct("alarm", "alarm_t", alarm_t),
                pvxs::members::Struct("timeStamp", "time_t", time_t)
            }),
            pvxs::members::StructA("tmit", {
                pvxs::members::Float32("value"),
                pvxs::members::Struct("alarm", "alarm_t", alarm_t),
                pvxs::members::Struct("timeStamp", "time_t", time_t)
            }),
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
    orbitValue["value.names"] = ns.freeze();
}

void PVAOrbitReceiver::setCompletedOrbit(const OrbitData& o) {
    printf("Setting a completed orbit!\n");
    
    pvxs::shared_array<pvxs::Value> xs(o.values.size());
    pvxs::shared_array<pvxs::Value> ys(o.values.size());
    pvxs::shared_array<pvxs::Value> tmits(o.values.size());
    auto xfield = orbitValue["value.x"];
    auto yfield = orbitValue["value.y"];
    auto tmitfield = orbitValue["value.tmit"];
    for (size_t i=0, N=o.values.size(); i<N; i++) {
        DBRValue xval = o.values[i][0];
        assert(xval->count == 1);
        xs[i] = xfield.allocMember();
        const epics::pvData::shared_vector<const double>& xval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(xval->buffer));
        xs[i]["value"] = xval_float_buffer[0];
        xs[i]["alarm.severity"] = xval->sevr;
        xs[i]["alarm.status"] = xval->stat;
        xs[i]["timeStamp.secondsPastEpoch"] = xval->ts.secPastEpoch;
        xs[i]["timeStamp.nanoseconds"] = xval->ts.nsec;
        
        DBRValue yval = o.values[i][1];
        assert(yval->count == 1);
        ys[i] = yfield.allocMember();
        const epics::pvData::shared_vector<const double>& yval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(yval->buffer));
        ys[i]["value"] = yval_float_buffer[0];
        ys[i]["alarm.severity"] = yval->sevr;
        ys[i]["alarm.status"] = yval->stat;
        ys[i]["timeStamp.secondsPastEpoch"] = yval->ts.secPastEpoch;
        ys[i]["timeStamp.nanoseconds"] = yval->ts.nsec;
        
        DBRValue tmitval = o.values[i][2];
        assert(tmitval->count == 1);
        tmits[i] = tmitfield.allocMember();
        const epics::pvData::shared_vector<const double>& tmitval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(tmitval->buffer));
        tmits[i]["value"] = tmitval_float_buffer[0];
        tmits[i]["alarm.severity"] = tmitval->sevr;
        tmits[i]["alarm.status"] = tmitval->stat;
        tmits[i]["timeStamp.secondsPastEpoch"] = tmitval->ts.secPastEpoch;
        tmits[i]["timeStamp.nanoseconds"] = tmitval->ts.nsec;
    }
    orbitValue["value.x"] = xs.freeze();
    orbitValue["value.y"] = ys.freeze();
    orbitValue["value.tmit"] = tmits.freeze();
    orbitValue["timeStamp.secondsPastEpoch"] = o.ts.secPastEpoch;
    orbitValue["timeStamp.nanoseconds"] = o.ts.nsec;
    auto newOrbitValue = orbitValue.clone();
    if (!pv->isOpen()) {
        pv->open(std::move(newOrbitValue));
    } else {
        pv->post(std::move(newOrbitValue));
    }
    orbitValue.unmark(); //Set all fields to unchanged in preparation for the next update.
    
}