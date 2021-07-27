/*
 * GenericOutput.h
 *
 * Created: 19/07/2021 18:10:09
 *  Author: GuavTek
 */ 


#ifndef GENERICOUTPUT_H_
#define GENERICOUTPUT_H_

#include "samd21.h"

enum class GOType_t {
	DCKey,
	DCCC,
	LFO,
	Envelope
};

struct GenOut_s {
	enum GOType_t type;
	uint32_t freq;
	uint32_t attack;
	uint32_t decay;
	uint32_t release;
	uint16_t sustain;
};

void GO_Init();

void GO_Service();



#endif /* GENERICOUTPUT_H_ */