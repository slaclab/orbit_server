#include "pva_orbit_receiver.h"
#include <pv/sharedVector.h>
#include <pvxs/data.h>
#include <db_access.h>


PVAOrbitReceiver::PVAOrbitReceiver(Orbit& orbit) :
orbit(orbit),
initialized(false)
{
    pv = std::make_shared<pvxs::server::SharedPV>(pvxs::server::SharedPV::buildReadonly());
    auto time_t = {
        pvxs::members::Int32("secondsPastEpoch"),
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
            pvxs::members::UInt16A("x_severity"),
            pvxs::members::UInt16A("x_status"),
            pvxs::members::Float64A("y_val"),
            pvxs::members::UInt16A("y_severity"),
            pvxs::members::UInt16A("y_status"),
            pvxs::members::Float64A("tmit_val"),
            pvxs::members::UInt16A("tmit_severity"),
            pvxs::members::UInt16A("tmit_status"),
        }),
        pvxs::members::String("descriptor"),
        pvxs::members::Struct("alarm", "alarm_t", alarm_t),
        pvxs::members::Struct("timeStamp", "time_t", time_t),
    }).create();
    pvxs::shared_array<std::string> labels({"device_name", "z", "x_val", "x_severity", "x_status", "x_ts_seconds", "x_ts_nanos", "y_val", "y_severity", "y_status", "y_ts_seconds", "y_ts_nanos", "tmit_val", "tmit_severity", "tmit_status", "tmit_ts_seconds", "tmit_ts_nanos"});
    orbitValue["labels"] = labels.freeze();
    orbitValue["descriptor"] = "LCLS Orbit Data";
    printf("Adding PVAOrbitReceiver to orbit.\n");
    orbit.add_receiver(this);
    printf("Leaving PVAOrbitReceiver initializer.\n");
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
    Guard G(mutex);
    pvxs::shared_array<double> x_val(o.values.size());
    pvxs::shared_array<uint16_t> x_severity(o.values.size());
    pvxs::shared_array<uint16_t> x_status(o.values.size());
    pvxs::shared_array<double> y_val(o.values.size());
    pvxs::shared_array<uint16_t> y_severity(o.values.size());
    pvxs::shared_array<uint16_t> y_status(o.values.size());
    pvxs::shared_array<double> tmit_val(o.values.size());
    pvxs::shared_array<uint16_t> tmit_severity(o.values.size());
    pvxs::shared_array<uint16_t> tmit_status(o.values.size());
    pvxs::shared_array<const double> last_xval(o.values.size());
    pvxs::shared_array<const double> last_yval(o.values.size());
    pvxs::shared_array<const double> last_tmitval(o.values.size());
    if (initialized) {
        last_xval = orbitValue["value"]["x_val"].as<pvxs::shared_array<const double>>();
        last_yval = orbitValue["value"]["y_val"].as<pvxs::shared_array<const double>>();
        last_tmitval = orbitValue["value"]["tmit_val"].as<pvxs::shared_array<const double>>();
    }
    for (size_t i=0, N=o.values.size(); i<N; i++) {
        DBRValue xval = o.values[i][0];
        if (xval.valid() && xval->sevr != 4) {
            assert(xval->count == 1);
            const epics::pvData::shared_vector<const double>& xval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(xval->buffer));
            x_val[i] = xval_float_buffer[0];
            x_severity[i] = xval->sevr;
            x_status[i]= xval->stat;
        } else {
            x_val[i] = last_xval.at(i);
            x_severity[i] = 4;
        }
        
        DBRValue yval = o.values[i][1];
        if (yval.valid() && yval->sevr != 4) {
            assert(yval->count == 1);
            const epics::pvData::shared_vector<const double>& yval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(yval->buffer));
            y_val[i] = yval_float_buffer[0];
            y_severity[i] = yval->sevr;
            y_status[i]= yval->stat;
        } else {
            y_val[i] = last_yval.at(i);
            y_severity[i] = 4;
        }
        
        DBRValue tmitval = o.values[i][2];
        if (tmitval.valid() && tmitval->sevr != 4) {
            assert(tmitval->count == 1);
            const epics::pvData::shared_vector<const double>& tmitval_float_buffer(epics::pvData::static_shared_vector_cast<const double>(tmitval->buffer));
            tmit_val[i] = tmitval_float_buffer[0];
            tmit_severity[i] = tmitval->sevr;
            tmit_status[i]= tmitval->stat;
        } else {
            tmit_val[i] = last_tmitval.at(i);
            tmit_severity[i] = 4;
        }
        
    }
    orbitValue["value.x_val"] = x_val.freeze();
    orbitValue["value.x_val"].mark();
    orbitValue["value.x_severity"] = x_severity.freeze();
    orbitValue["value.x_severity"].mark();
    orbitValue["value.x_status"] = x_status.freeze();
    orbitValue["value.x_status"].mark();
    
    orbitValue["value.y_val"] = y_val.freeze();
    orbitValue["value.y_val"].mark();
    orbitValue["value.y_severity"] = y_severity.freeze();
    orbitValue["value.y_severity"].mark();
    orbitValue["value.y_status"] = y_status.freeze();
    orbitValue["value.y_status"].mark();
    
    orbitValue["value.tmit_val"] = tmit_val.freeze();
    orbitValue["value.tmit_val"].mark();
    orbitValue["value.tmit_severity"] = tmit_severity.freeze();
    orbitValue["value.tmit_severity"].mark();
    orbitValue["value.tmit_status"] = tmit_status.freeze();
    orbitValue["value.tmit_status"].mark();
    
    orbitValue["timeStamp.secondsPastEpoch"] = o.ts.secPastEpoch;
    orbitValue["timeStamp.secondsPastEpoch"].mark();
    orbitValue["timeStamp.nanoseconds"] = o.ts.nsec;
    orbitValue["timeStamp.nanoseconds"].mark();
    auto newOrbitValue = orbitValue.clone();
    {
        if (!pv->isOpen()) {
            pv->open(std::move(newOrbitValue));
        } else {
            pv->post(std::move(newOrbitValue));
        }
    }
    initialized = true;
    orbitValue.unmark(); //Set all fields to unchanged in preparation for the next update.
    
}
