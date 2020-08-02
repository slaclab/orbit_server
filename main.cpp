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

#include "orbit.h"
#include "pva_orbit_receiver.h"

int main (int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stdout, "Usage: %s [MODEL_PV] [EDEF]\n", argv[0]);
        return 1;
    }
    //auto filename = std::string(argv[1]);
    printf("About to start.\n");
    std::vector<std::string> bpm_names;
    //std::ifstream input_file(argv[1]);
    //for(std::string line; std::getline(input_file, line);) {
    //    bpm_names.emplace_back(line);
    //}
    auto model_pv = std::string(argv[1]);
    auto edef = std::string(argv[2]);
    
    //Now, get all the BPM Z values from the model.
    pvxs::client::Context pva_ctxt = pvxs::client::Config::from_env().build();
    auto model_table = pva_ctxt.get(model_pv).exec()->wait().clone();
    pvxs::shared_array<const void> name_col = model_table["value"]["device_name"].as<pvxs::shared_array<const void>>();
    pvxs::shared_array<const std::string> name_vals = name_col.castTo<const std::string>();
    pvxs::shared_array<const void> z_col = model_table["value"]["s"].as<pvxs::shared_array<const void>>();
    pvxs::shared_array<const double> z_vals = z_col.castTo<const double>();
    std::vector<double> bpm_z_vals;
    for (size_t i=0, N = z_vals.size(); i<N; i++) {
        if (name_vals[i].rfind("BPMS", 0) == 0) {
            bpm_names.push_back(name_vals[i]);
            bpm_z_vals.push_back(z_vals[i]);
        }
    }
    assert(bpm_z_vals.size() == bpm_names.size());
    
    fprintf(stdout, "Connecting to BPMs...\n");
    std::shared_ptr<CAContext> context;
    context.reset(new CAContext(epicsThreadPriorityMedium));
    auto orbit = new Orbit(*context, bpm_names, bpm_z_vals, edef);
    auto receiver = new PVAOrbitReceiver(*orbit);
    auto connected = orbit->wait_for_connection(std::chrono::seconds(3));
    auto server = pvxs::server::Config::from_env().build().addPV("ORBIT:TABLE", *(receiver->pv));

    if (!connected) {
        printf("Connection timeout exceeded.\n");
        return 1;
    }
    printf("Done connecting. Spinning up PVA server.\n");
    server.run();
    //ca_context_destroy();
    return 0;
}