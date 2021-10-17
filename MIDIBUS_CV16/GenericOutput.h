/*
 * GenericOutput.h
 *
 * Created: 19/07/2021 18:10:09
 *  Author: GuavTek
 */ 


#ifndef GENERICOUTPUT_H_
#define GENERICOUTPUT_H_

#include "samd21.h"
#include "MIDI_Driver.h"

enum class GOType_t {
	DC,
	LFO,
	Envelope,
	CLK,
	Pressure,
	Velocity,
	Gate
};

enum class ctrlType_t {
	None,
	Key,
	CC
};

struct ctrlSource_t {
	enum ctrlType_t sourceType;
	uint8_t channel;
	uint16_t sourceNum;
};

enum class WavShape_t {
	Square,
	Triangle,
	Sawtooth,
	Sine
};

// The data to store in NVM
struct GenOut_base {
	enum GOType_t type;
	uint16_t max_range;
	uint16_t min_range;
	union {
		// DC (CC, pressure, velocity, gate, and keys)
		struct {
			struct ctrlSource_t dc_source;
		};
		
		// LFO (and clk)
		struct {
			enum WavShape_t shape;
			uint32_t freq_max;
			uint32_t freq_min;
			struct ctrlSource_t freq_source;
		};
		
		// Envelope
		struct {
			struct ctrlSource_t env_source;
			uint8_t att_max;
			uint8_t att_min;
			struct ctrlSource_t att_source;
			uint8_t dec_max;
			uint8_t dec_min;
			struct ctrlSource_t dec_source;
			uint16_t sus_max;
			uint16_t sus_min;
			struct ctrlSource_t sus_source;
			uint8_t rel_max;
			uint8_t rel_min;
			struct ctrlSource_t rel_source;
		};
		
	};
	
};

// The data to store in RAM
struct GenOut_t : GenOut_base {
	uint16_t currentOut;
	union {
		struct {
			uint32_t freq_current;
			uint32_t freq_count;
			int8_t direction;
		};
		
		struct {
			uint8_t envelope_stage;
			uint16_t att_current;
			uint16_t dec_current;
			uint16_t sus_current;
			uint16_t rel_current;
		};
	};
};



void GO_Init();

void GO_Service();

void GO_MIDI_Voice(MIDI2_voice_t* msg);

void GO_MIDI_Realtime(MIDI2_com_t* msg);

uint16_t TriSine(uint16_t in);

#endif /* GENERICOUTPUT_H_ */