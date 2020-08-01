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

#include "orbit.h"
#include "pva_orbit_receiver.h"

int main (int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stdout, "Usage: %s [BPM file] [EDEF]\n", argv[0]);
        return 1;
    }
    ca_context_create(ca_enable_preemptive_callback);
    auto filename = std::string(argv[1]);
    printf("About to start.\n");
    std::vector<std::string> bpm_names;
    std::ifstream input_file(argv[1]);
    for(std::string line; std::getline(input_file, line);) {
        //printf("%s\n", line.c_str());
        bpm_names.emplace_back(line);
    }
    auto edef = std::string("");
    fprintf(stdout, "Connecting to BPMs...\n");
    std::shared_ptr<CAContext> context;
    context.reset(new CAContext(epicsThreadPriorityMedium));
    auto orbit = new Orbit(*context, bpm_names, "");
    auto receiver = new PVAOrbitReceiver(*orbit);
    auto connected = orbit->wait_for_connection(std::chrono::seconds(3));
    auto server = pvxs::server::Config::from_env().build().addPV("ORBIT:TABLE", *(receiver->pv));
    /*
    epicsEvent done;
    pvxs::SigInt handle([&done]() {
        done.signal();
    });
    */
    if (!connected) {
        printf("Connection timeout exceeded.\n");
        return 1;
    }
    printf("Done connecting. Spinning up PVA server.\n");
    server.run();
    //ca_context_destroy();
    return 0;
}