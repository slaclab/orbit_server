#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include "bpm.h"

int main (int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stdout, "Usage: %s [PV] [EDEF]\n", argv[0]);
		return 1;
	}
	ca_context_create(ca_enable_preemptive_callback);
	auto bpmName = std::string(argv[1]);
	auto edef = std::string(argv[2]);
	fprintf(stdout, "Connecting to BPM...");
	auto bpm = BPM(bpmName, edef);
	int counter = 0;
	while (counter < 1000) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		fprintf(stdout, "X: %g, Y: %g, TMIT: %g\n", bpm.x(), bpm.y(), bpm.tmit());
		counter++;
	}
	printf("Done!");
	//ca_context_destroy();
	return 0;
}