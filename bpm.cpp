#include "bpm.h"

BPM::BPM(const std::string bpm_name, const std::string edef_suffix) : 
name(bpm_name),
edef(edef_suffix)
{
    printf("Making BPM: %s\n", bpm_name.c_str());
    xpv = PV(bpm_name + ":X" + edef_suffix);
    ypv = PV(bpm_name + ":Y" + edef_suffix);
    tmitpv = PV(bpm_name + ":TMIT" + edef_suffix);
}

bool BPM::connected() const {
    return (xpv.connected && ypv.connected && tmitpv.connected);
}

std::string BPM::bpm_name() const {
    return name;
}

std::string BPM::edef_suffix() const {
    return edef;
}

float BPM::x() const {
    return (float)xpv->value();
}

float BPM::y() const {
    return (float)ypv->value();
}

float BPM::tmit() const {
    return (float)tmitpv->value();
}
