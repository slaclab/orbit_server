#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <string>
#include <epicsThread.h>
#include <pvxs/server.h>
#include <pvxs/util.h>
#include <pvxs/client.h>
#include <pvxs/log.h>
#include "orbit.h"
#include "pva_orbit_receiver.h"

int main (int argc, char *argv[]) {
    bool fakeOrbitMode = false;
    if (argc == 3 && strcmp(argv[1], "--fake") == 0) {
        fakeOrbitMode = true;
    } else if (argc < 4) {
        fprintf(stdout, "Usage: %s [MODEL_PV] [EDEF] [OUTPUT_PV]\n", argv[0]);
        return 1;
    }

    pvxs::logger_config_env();
    std::vector<std::string> bpm_names;
    std::string output_pv;
    std::string edef;
    std::vector<double> bpm_z_vals;
    if (!fakeOrbitMode) {
        auto model_pv = std::string(argv[1]);
        edef = std::string(argv[2]);
        output_pv = std::string(argv[3]);
        
        //Now, get all the BPM Z values from the model.
        pvxs::client::Context pva_ctxt = pvxs::client::Config::from_env().build();
        auto model_table = pva_ctxt.get(model_pv).exec()->wait(4.0).clone();
        pvxs::shared_array<const void> name_col = model_table["value"]["device_name"].as<pvxs::shared_array<const void>>();
        pvxs::shared_array<const std::string> name_vals = name_col.castTo<const std::string>();
        pvxs::shared_array<const void> z_col = model_table["value"]["s"].as<pvxs::shared_array<const void>>();
        pvxs::shared_array<const double> z_vals = z_col.castTo<const double>();
        for (size_t i=0, N = z_vals.size(); i<N; i++) {
            if (name_vals[i].rfind("BPMS", 0) == 0) {
                bpm_names.push_back(name_vals[i]);
                bpm_z_vals.push_back(z_vals[i]);
            }
        }
    } else {
        output_pv = std::string(argv[2]);
        edef = std::string("");
        std::ostringstream nameStream;
        for (size_t i=0, N = 101; i<N; i++) {
            nameStream.str("");
            nameStream << "BPMS:LTUH:" << i;
            printf("%s\n", nameStream.str().c_str());
            bpm_names.push_back(nameStream.str());
            bpm_z_vals.push_back(float(i));
        }
    }
    assert(bpm_z_vals.size() == bpm_names.size());
    fprintf(stdout, "Connecting to BPMs...\n");
    std::shared_ptr<CAContext> context;
    context.reset(new CAContext(epicsThreadPriorityMedium));
    auto orbit = new Orbit(*context, bpm_names, bpm_z_vals, edef);
    printf("Orbit initialized.\n");
    auto receiver = new PVAOrbitReceiver(*orbit);
    printf("Receiver initialized.\n");
    auto server = pvxs::server::Config::from_env().build().addPV(output_pv, *(receiver->pv));
    printf("Done connecting. Spinning up PVA server.\n");
    server.run();
    return 0;
}
