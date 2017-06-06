#include "bpm.h"

BPM::BPM(const std::string& bpm_name, const std::string& edef_suffix) {
	this->name = bpm_name;
	this->edef = edef_suffix;
	connect(); //Establishes channel access connections.	
}

BPM::~BPM() {
	ca_clear_channel(xpv.chan);
	ca_clear_channel(ypv.chan);
	ca_clear_channel(tmitpv.chan);
}

void BPM::connect() {
	xpv.pvname = name + ":X" + edef;
	xpv.val = &xval;
	ca_create_channel(xpv.pvname.c_str(), bpmConnectionCallback, &xpv, 0, &(xpv.chan));
	ypv.pvname = name + ":Y" + edef;
	ypv.val = &yval;
	ca_create_channel(ypv.pvname.c_str(), bpmConnectionCallback, &ypv, 0, &(ypv.chan));
	tmitpv.pvname = name + ":TMIT" + edef;
	tmitpv.val = &tmitval;
	ca_create_channel(tmitpv.pvname.c_str(), bpmConnectionCallback, &tmitpv, 0, &(tmitpv.chan));
}

void BPM::bpmConnectionCallback(connection_handler_args args) {
	if (args.op == CA_OP_CONN_UP) {
		PV<float>* bpm_pv = (PV<float>*)ca_puser(args.chid);
		ca_create_subscription(DBR_FLOAT, 1, args.chid, DBE_VALUE|DBE_ALARM, bpmMonitorCallback, bpm_pv, &(bpm_pv->ev));
	}
}

void BPM::bpmMonitorCallback(event_handler_args args) {
	PV<float>* bpm_pv = (PV<float>*)args.usr;
	dbr_float_t* data = (dbr_float_t*)args.dbr;
	*(bpm_pv->val) = (float) *data; //set xval, yval, or tmitval to the new value
}

bool BPM::connected() {
	auto x_state = ca_state(xpv.chan);
	auto y_state = ca_state(ypv.chan);
	auto tmit_state = ca_state(tmitpv.chan);
	return (x_state == cs_conn && y_state == cs_conn && tmit_state == cs_conn);
}

std::string BPM::bpm_name() {
	return this->name;
}

std::string BPM::edef_suffix() {
	return this->edef;
}

float BPM::x() {
	return xval;
}

float BPM::y() {
	return yval;
}

float BPM::tmit() {
	return tmitval;
}
