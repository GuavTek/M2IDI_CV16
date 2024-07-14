/*
 * GenericOutput.h
 *
 * Created: 19/07/2021 18:10:09
 *  Author: GuavTek
 */ 


#ifndef GENERIC_OUTPUT_H_
#define GENERIC_OUTPUT_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "umpProcessor.h"

enum class GOType_t : uint8_t {
	DC = 0,
	LFO = 1,
	Envelope = 2,
	CLK = 3,
	Pressure = 4,
	Velocity = 5,
	Gate = 6
};

enum class ctrlType_t : uint8_t {
	none,
	key,
	controller,
	program
};

// TODO? this could subscribe to key handler/CV handler etc
struct ctrlSource_t {
	enum ctrlType_t sourceType;
	uint8_t channel;
	uint16_t sourceNum;
};


enum class WavShape_t : uint8_t {
	Square = 0,
	Triangle = 1,
	Sawtooth = 2,
	Sine = 3,
	SinSaw = 4
};

struct GridPos_t {
	uint8_t x : 2;
	uint8_t y : 2;
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

class base_output_c{
	public:
		virtual void update(GenOut_t* go){};
		virtual void handle_realtime(GenOut_t* go, umpGeneric* msg){};
		virtual void handle_cvm(GenOut_t* go, umpCVM* msg){};
		static uint16_t TriSine(uint16_t in);
};

class dc_output_c : public base_output_c{
	public:
	//void update(GenOut_t* go);
	//void handle_realtime(GenOut_t* go, umpGeneric* msg);
	void handle_cvm(GenOut_t* go, umpCVM* msg);
};

class lfo_output_c : public base_output_c{
	public:
	void update(GenOut_t* go);
	//void handle_realtime(GenOut_t* go, umpGeneric* msg);
	void handle_cvm(GenOut_t* go, umpCVM* msg);
};

class envelope_output_c : public base_output_c{
	public:
	void update(GenOut_t* go);
	//void handle_realtime(GenOut_t* go, umpGeneric* msg);
	void handle_cvm(GenOut_t* go, umpCVM* msg);
};

class clk_output_c : public base_output_c{
	public:
	//void update(GenOut_t* go);
	void handle_realtime(GenOut_t* go, umpGeneric* msg);
	//void handle_cvm(GenOut_t* go, umpCVM* msg);
};

class pressure_output_c : public base_output_c{
	public:
	//void update(GenOut_t* go);
	//void handle_realtime(GenOut_t* go, umpGeneric* msg);
	void handle_cvm(GenOut_t* go, umpCVM* msg);
};

class velocity_output_c : public base_output_c{
	public:
	//void update(GenOut_t* go);
	//void handle_realtime(GenOut_t* go, umpGeneric* msg);
	void handle_cvm(GenOut_t* go, umpCVM* msg);
};

class gate_output_c : public base_output_c{
	public:
	//void update(GenOut_t* go);
	//void handle_realtime(GenOut_t* go, umpGeneric* msg);
	void handle_cvm(GenOut_t* go, umpCVM* msg);
};

class generic_output_c {
	public:
	void update();
	void handle_realtime(umpGeneric* msg);
	void handle_cvm(umpCVM* msg);
	void set_type(GOType_t type);
	void set_key_lane(uint8_t lane) {key_lane = lane;}
	uint8_t get_key_lane() {return key_lane;}
	inline uint16_t get() {return state.currentOut;};
	GenOut_t state;
	generic_output_c() {current_handler = &dc_handler;};
	protected:
	uint8_t num_cc;
	uint8_t key_lane;
	base_output_c* current_handler;
	static dc_output_c dc_handler;
	static lfo_output_c lfo_handler;
	static envelope_output_c envelope_handler;
	static clk_output_c clk_handler;
	static pressure_output_c pressure_handler;
	static velocity_output_c velocity_handler;
	static gate_output_c gate_handler;
};

typedef struct key_note_t {
	int8_t note;
};

class key_handler_c {
	public:
	void reset();
	void stop_notes();
	inline void set_key_channel(uint8_t num) {channel = num;}
	inline uint8_t get_key_channel() {return channel;}
	void set_bend_range(uint8_t range);
	inline int16_t get_current_bend() {return current_bend;}
	uint8_t handle_cvm(umpCVM* msg);
	uint8_t subscribe_key(generic_output_c* handler);
	uint8_t subscribe_key(generic_output_c* handler, uint8_t lane);
	uint8_t subscribe_drum(generic_output_c* handler);

	protected:
	void start_note(uint8_t lane, umpCVM* msg);
	void start_note(umpCVM* msg);
	void stop_note(uint8_t lane, umpCVM* msg);
	void stop_note(umpCVM* msg);
	uint8_t queue_index;
	key_note_t note_queue[32];
	uint8_t next_lane = 0;
	int16_t current_bend = 0;
	uint16_t max_bend;
	uint16_t min_bend;
	uint8_t channel;
	uint8_t num_lanes;			// Number of lanes
	int8_t drum_note[8];
	uint8_t num_outputs[8];	// Output number per lane
	uint8_t num_coms;
	int8_t key_playing[8];		// The currently playing note on each lane (-1 for none)
	generic_output_c* com_out[16];
	generic_output_c* lanes[8][4];	// Outputs subscribed to each lane
};

const uint32_t out_rate = 22100;	// The rate each output is updated
extern bool needScan;
extern key_handler_c key_handler;
extern generic_output_c out_handler[4][4];
extern Env_t envelopes[4];
extern uint8_t midi_group;

// TODO: remove
extern uint8_t bendRange;

void GO_Init();

void GO_Service();
void GO_Service(uint8_t x);

void GO_MIDI_Voice(struct umpCVM* msg);

void GO_MIDI_Realtime(struct umpGeneric* msg);

#endif /* GENERIC_OUTPUT_H_ */