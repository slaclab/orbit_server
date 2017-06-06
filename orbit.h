#ifndef ORBIT_H
#define ORBIT_H

#include <vector>
#include <string>
#include "cadef.h"

int main(int argc, char *argv[]);

template <typename T>
struct PV {
	std::string pvname;
	chid chan;
	evid ev;
	T* val;
};

class BPM {
public:
BPM(const std::string bpm_name, const std::string edef_suffix);
	BPM(const BPM&) = default;
	BPM& operator=(const BPM&) = default;
	BPM(BPM&&) = default;
	BPM& operator=(BPM&&) = default;
	~BPM();
	std::string bpm_name();
	std::string edef_suffix();
	bool connected();
	float x();
	float y();
	float tmit();
private:
	std::string name;
	std::string edef;
	PV<float> xpv;
	PV<float> ypv;
	PV<float> tmitpv;
	float xval;
	float yval;
	float tmitval;
	void connect();
	static void bpmConnectionCallback(struct connection_handler_args args);
	static void bpmMonitorCallback(struct event_handler_args args);
};
/*
class Orbit {
public:
	Orbit(std::vector<std::string> bpm_names);
	std::vector<std::string> names();
	std::vector<BPM>::size_type count();
	std::vector<BPM>::iterator bpmBegin();
	std::vector<BPM>::iterator bpmEnd();
	void connect();
	bool connected();
private:
	std::vector<BPM> bpms;
};
*/
#endif //ORBIT_H
