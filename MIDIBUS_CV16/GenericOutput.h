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

enum class GOType_t : uint8_t {
	DC,
	LFO,
	Envelope,
	CLK,
	Pressure,
	Velocity,
	Gate
};

enum class ctrlType_t : uint8_t {
	None,
	Key,
	CC,
	PC
};

struct ctrlSource_t {
	enum ctrlType_t sourceType;
	uint8_t channel;
	uint16_t sourceNum;
};

enum class WavShape_t : uint8_t {
	Square,
	Triangle,
	Sawtooth,
	Sine
};

// The data to store in NVM
struct GenOut_base {
	enum GOType_t type = GOType_t::DC;
	uint16_t max_range;
	uint16_t min_range;
	struct ctrlSource_t gen_source;
	union {
		// DC (CC, pressure, velocity, gate, and keys)
		struct {
			
		};
		
		// LFO (and clk)
		struct {
			enum WavShape_t shape;
			uint32_t freq_max;
			uint32_t freq_min;
		};
		
		// Envelope
		struct {
			uint8_t env_num;
			
		};
		
	};
	
};

// The data to store in RAM
struct GenOut_t : GenOut_base {
	uint16_t currentOut;
	uint32_t outCount;
	union {
		struct {
			uint32_t freq_current;
			int8_t direction;
		};
		
		struct {
			uint8_t envelope_stage;
		};
	};
};

// Envelope data saved in NVM
struct Env_base {
	uint8_t att_max;
	uint8_t att_min;
	struct ctrlSource_t att_source;
	uint8_t dec_max;
	uint8_t dec_min;
	struct ctrlSource_t dec_source;
	uint8_t sus_max;
	uint8_t sus_min;
	struct ctrlSource_t sus_source;
	uint8_t rel_max;
	uint8_t rel_min;
	struct ctrlSource_t rel_source;
};

// Envelope data to store in RAM
struct Env_t : Env_base {
	uint32_t att_max;
	uint32_t att_min;
	uint32_t dec_max;
	uint32_t dec_min;
	uint16_t sus_max;
	uint16_t sus_min;
	uint32_t rel_max;
	uint32_t rel_min;
	uint32_t att_current;
	uint32_t dec_current;
	uint16_t sus_current;
	uint32_t rel_current;
};

// Collection of data saved in NVM
struct ConfigNVM_t {
	uint8_t bendRange;
	GenOut_base matrix[4][4];
	Env_base env[4];
};

extern bool needScan;
extern uint8_t bendRange;
extern GenOut_t outMatrix[4][4];
extern Env_t envelopes[4];
extern uint8_t midi_group;

void GO_Init();

void GO_Service();

void GO_MIDI_Voice(MIDI2_voice_t* msg);

void GO_MIDI_Realtime(MIDI2_com_t* msg);

uint16_t TriSine(uint16_t in);

#endif /* GENERICOUTPUT_H_ */