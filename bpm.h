#ifndef BPM_H
#define BPM_H

#include <vector>
#include <string>
#include "cadef.h"

template <typename T>
struct PV {
	std::string pvname;
	chid chan;
	evid ev;
	T* val;
};

class BPM {
public:
	BPM(const std::string& bpm_name, const std::string& edef_suffix);
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

#endif //BPM_H