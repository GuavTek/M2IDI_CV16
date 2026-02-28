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

enum class EnvStage_t : uint8_t {
	idle = 0,
	attack = 1,
	decay = 2,
	sustain = 3,
	release = 4
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
	SinSaw = 4,
	SuperSaw = 5,
	Noise = 6,
	randGate = 7
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
	uint8_t key_lane;
	struct ctrlSource_t gen_source;
	union {
		// DC (CC, pressure, velocity, gate, and keys)
		struct {
		};

		// LFO (and clk)
		struct {
			enum WavShape_t shape;
			uint8_t freq_max;
			uint8_t freq_min;
			uint8_t mod_max;
			uint8_t mod_min;
			struct ctrlSource_t mod_source;
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
			uint32_t freq_max;
			uint32_t freq_min;
			uint32_t mod_current;	// For square width etc
			uint32_t mod_max;
			uint32_t mod_min;
			int8_t direction;
		};

		struct {
			EnvStage_t envelope_stage;
			uint32_t env_value;
			uint16_t env_velocity;
			uint16_t env_sustain;
		};
	};
};

struct Env_stage_base {
	struct ctrlSource_t source;
	uint8_t max;
	uint8_t min;
};

// Envelope data saved in NVM
struct Env_base {
	uint8_t dec_disable : 1;
	uint8_t sus_disable : 1;
	Env_stage_base att;
	Env_stage_base dec;
	Env_stage_base sus;
	Env_stage_base rel;
};

struct Env_stage_t {
	struct ctrlSource_t source;
	uint32_t max;
	uint32_t min;
	uint32_t current;
	uint8_t disable;
};

// Envelope data to store in RAM
struct Env_t {
	Env_stage_t att;
	Env_stage_t dec;
	Env_stage_t sus;
	Env_stage_t rel;
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
	void set_key_lane(uint8_t lane) {state.key_lane = lane;}
	uint8_t get_key_lane() {return state.key_lane;}
	inline uint16_t get() {return state.currentOut;};
	GenOut_t state;
	generic_output_c() {current_handler = &dc_handler;};
	protected:
	uint8_t num_cc;
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
	uint8_t get_bend_range();
	inline int16_t get_current_bend() {return current_bend;}
	inline int16_t get_current_bend(uint8_t lane){
		if (lane == 0) return 0;
		return bend_per_note[lane_map[lane-1]];}
	uint8_t handle_cvm(umpCVM* msg);
	uint8_t subscribe_key(generic_output_c* handler);
	uint8_t subscribe_key(generic_output_c* handler, uint8_t lane);
	uint8_t subscribe_drum(generic_output_c* handler);
	uint8_t lane_map[8];
	protected:
	void start_note(uint8_t lane, umpCVM* msg);
	void start_note(umpCVM* msg);
	void stop_note(uint8_t lane, umpCVM* msg);
	void stop_note(umpCVM* msg);
	uint8_t handle_cvm_key(umpCVM* msg);
	uint8_t handle_cvm_drum(umpCVM* msg);
	uint8_t queue_index;
	key_note_t note_queue[32];
	uint8_t next_lane = 0;
	int16_t current_bend = 0;
	int16_t bend_per_note[8];
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

class env_handler_c {
	public:
	void handle_cvm(umpCVM* msg);
	uint32_t get(EnvStage_t stage, uint16_t vel);
	void set_go(GenOut_t* handler, EnvStage_t stage);
	bool enabled(EnvStage_t stage);
	Env_t env;
	protected:
	uint32_t get_stage(uint32_t val, Env_stage_t* stage);
	void set_stage(uint32_t val, Env_stage_t* stage);
};

const uint8_t DRUM_CHANNEL = 9; // Channel 10 in MIDI, used for drums
const uint32_t out_rate = 22100;	// The rate each output is updated
extern bool needScan;
extern key_handler_c key_handler;
extern generic_output_c out_handler[4][4];
extern env_handler_c envelopes[4];
extern uint8_t midi_group;

void GO_Init();

void GO_Service();
void GO_Service(uint8_t x);

void GO_Get_Config(ConfigNVM_t* conf);
void GO_Set_Config(ConfigNVM_t* conf);
void GO_Default_Config();

void GO_MIDI_Voice(struct umpCVM* msg);

void GO_MIDI_Realtime(struct umpGeneric* msg);

#endif /* GENERIC_OUTPUT_H_ */