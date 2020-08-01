#ifndef BPM_H
#define BPM_H

#include <vector>
#include <string>
#include <utility>
#include <memory>
#include "cadef.h"
#include "pv.cpp"

class BPM {
public:
	//BPM();
	BPM(const std::string& bpm_name, const std::string& edef_suffix);
	std::string bpm_name() const;
	std::string edef_suffix() const;
	bool connected() const;
	float x() const;
	float y() const;
	float tmit() const;
private:
	std::string name;
	std::string edef;
	PV xpv;
	PV ypv;
	PV tmitpv;
};

#endif //BPM_H