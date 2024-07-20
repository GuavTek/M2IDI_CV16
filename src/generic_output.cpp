/*
 * generic_output.cpp
 *
 * Created: 19/07/2021 18:09:51
 *  Author: GuavTek
 */ 

#include "generic_output.h"
#include "led_matrix.h"
#include "umpProcessor.h"
#include "utils.h"
#include "math.h"

bool needScan = false;

Env_t envelopes[4];

uint8_t bendRange; // TODO: remove and fix bendrange editing

dc_output_c generic_output_c::dc_handler = dc_output_c();
lfo_output_c generic_output_c::lfo_handler = lfo_output_c();
envelope_output_c generic_output_c::envelope_handler = envelope_output_c();
clk_output_c generic_output_c::clk_handler = clk_output_c();
pressure_output_c generic_output_c::pressure_handler = pressure_output_c();
velocity_output_c generic_output_c::velocity_handler = velocity_output_c();
gate_output_c generic_output_c::gate_handler = gate_output_c();

generic_output_c out_handler[4][4];
key_handler_c key_handler = key_handler_c();

// Generate constant array for genout oscillator frequencies
constexpr struct freqs_t {
    constexpr freqs_t() : midi() {
        for (auto i = 0; i < 128; i++){
            midi[i] = 440.0 * pow(2.0, (i-69)/12.0) * pow(2.0, 32.0) / out_rate; 
		}
    }
    uint32_t midi[128];
	uint32_t f1Hz = pow(2.0, 32.0)/out_rate;
} FREQS;

//template <uint32_t i>
//struct PrintConst;
//PrintConst<FREQS.midi[8]> p;

// hasCC[4] is for envelopes
bool hasCC[5][4];

uint8_t midi_group = 1;

#define ENV_MANTISSA 7

// TODO: update gain if switching to DACs
#define OUTPUT_GAIN 1		// Nominal gain
#define INT_PER_VOLT 6553.6/OUTPUT_GAIN
#define INT_PER_NOTE INT_PER_VOLT/12
#define FIXED_POINT_POS 16
#define FIXED_VOLT_PER_INT ((uint32_t) ((1/INT_PER_VOLT) * (1 << FIXED_POINT_POS)))
#define FIXED_INT_PER_NOTE ((uint32_t) (INT_PER_NOTE * (1 << FIXED_POINT_POS)))

// Scan the configuration
// To configure key_handler
void Scan_Matrix(){
	key_handler.reset();

	// find Keychannel and which lanes are present
	bool found_channel = false;
	uint8_t active_lanes = 0;
	for(uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::key){
				if (out_handler[x][y].get_key_lane() > 0){
					active_lanes |= 1 << (out_handler[x][y].get_key_lane()-1);
				}
				if (found_channel) continue;
				if (out_handler[x][y].state.gen_source.channel != 9){
					key_handler.set_key_channel(out_handler[x][y].state.gen_source.channel);
					found_channel = true;
				}
			}
		}
	}

	// Map the active keylanes to the handler lanes
	uint8_t lane_map[9];
	lane_map[0] = 0;
	uint8_t lane_num = 1;
	for (uint8_t i = 0; i < 8; i++){
		if(active_lanes & (1 << i)){
			lane_map[i+1] = lane_num++;
		} else {
			lane_map[i+1] = 0;
		}
	}
	
	// Subscribe to keylanes
	for(uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::key){
				if (out_handler[x][y].state.gen_source.channel == key_handler.get_key_channel()){
					key_handler.subscribe_key(&out_handler[x][y], lane_map[out_handler[x][y].get_key_lane()]);
				} else if (out_handler[x][y].state.gen_source.channel == 9){
					key_handler.subscribe_drum(&out_handler[x][y]);
				}
			}
		}
	}
	
	// Find controller bound to outputs
	for (uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::none){
				hasCC[x][y] = 0;
			} else {
				hasCC[x][y] = 1;
			}
		}
	}
	
	// Detect controller in envelopes
	for (uint8_t i = 0; i < 4; i++){
		hasCC[4][i] = 0;
		if (envelopes[i].att_source.sourceType != ctrlType_t::none){
			hasCC[4][i] = 1;
			continue;
		}
		if (envelopes[i].dec_source.sourceType != ctrlType_t::none){
			hasCC[4][i] = 1;
			continue;
		}
		if (envelopes[i].sus_source.sourceType != ctrlType_t::none){
			hasCC[4][i] = 1;
			continue;
		}
		if (envelopes[i].rel_source.sourceType != ctrlType_t::none){
			hasCC[4][i] = 1;
			continue;
		}
	}
	
	
	needScan = false;
}

// Low precision floating point to save memory space
#define ufloat8_t uint8_t
uint32_t ufloat8_to_uint32(ufloat8_t in){
	uint8_t exp = in >> 3;
	uint8_t mant = in & 0x07;
	if (exp > 29){
		exp = 29;
	}
	if (exp > 0){
		mant |= 0x08;
	}
	return mant << exp;
}

ufloat8_t uint32_to_ufloat8(uint32_t in){
	uint8_t exp = 29;
	uint8_t mant = 0;
	for (; exp > 0; exp--){
		mant = (in >> exp) & 0x0f;
		if (mant >= 8){
			break;
		}
	}
	return ((mant & 0x07) | (exp << 3));
}

inline uint16_t Note_To_Output(uint8_t note){
	int32_t tempOut = note * FIXED_INT_PER_NOTE;// * 546.133 fixed point multiplication
	tempOut += 1 << (FIXED_POINT_POS - 1);	// + 0.5 to round intead of floor
	tempOut >>= FIXED_POINT_POS;	// Round to int
	tempOut += 0x7fff - (uint16_t) (60 * INT_PER_NOTE);	// C4 is middle note -> 60 = 0V
	
	// Add bend
	tempOut += key_handler.get_current_bend();
	
	if (tempOut < 0){
		tempOut = 0;
	} else if (tempOut > 0xffff){
		tempOut = 0xffff;
	}
	
	return (uint16_t) tempOut;
}

// Scales a value
inline uint16_t Rescale_16bit(uint16_t val, uint16_t minOut, uint16_t maxOut){
	uint32_t tempResult = 0;
	if (maxOut > minOut) {
		tempResult = (maxOut-minOut+1) * val;
		tempResult >>= 16; // Divide by input range
		tempResult += minOut;
	} else if (minOut > maxOut) {	// TODO: make sure this can't happen
		tempResult = (minOut-maxOut+1) * val;
		tempResult >>= 16; // Divide by input range
		tempResult = minOut - tempResult;
	}
	return (uint16_t) tempResult;
}

inline void Reset_All_Controllers(){
	for(uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			uint32_t src_current = ( uint8_t(out_handler[x][y].state.type) << 0 ) | ( uint8_t(out_handler[x][y].state.gen_source.sourceType) << 8 );
			uint32_t criteria = ( uint8_t(GOType_t::DC) << 0 ) | ( uint8_t(ctrlType_t::controller) << 8 );
			if ( src_current == criteria ){
				out_handler[x][y].state.currentOut = out_handler[x][y].state.min_range;
			}
		}
	}
}

void GO_Init(){
	// Set default values
	for (uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			out_handler[x][y].state.currentOut = 0x7000;
			out_handler[x][y].state.max_range = 0xffff;
			out_handler[x][y].state.min_range = 0;
		}
	}
	
	// Temporary settings
	out_handler[0][3].set_type(GOType_t::LFO);
	out_handler[0][3].state.shape = WavShape_t::Sawtooth;
	out_handler[0][3].state.max_range = 0x7fff;
	out_handler[0][3].state.min_range = 0;
	out_handler[0][3].state.direction = -1;
	out_handler[0][3].state.freq_current = 0x0008 << 16;
	out_handler[1][2].set_type(GOType_t::LFO);
	out_handler[1][2].state.shape = WavShape_t::Sawtooth;
	out_handler[1][2].state.max_range = 0xffff;
	out_handler[1][2].state.min_range = 0x7fff;
	out_handler[1][2].state.direction = -1;
	out_handler[1][2].state.freq_current = 0x0010 << 16;
	out_handler[1][2].state.freq_max = 0x01000000;
	out_handler[1][2].state.freq_min = 0x00001000;
	out_handler[1][2].state.gen_source.channel = 1;
	out_handler[1][2].state.gen_source.sourceNum = 10;
	out_handler[1][2].state.gen_source.sourceType = ctrlType_t::controller;
	hasCC[1][2] = 1;
	
	out_handler[0][1].set_type(GOType_t::LFO);
	out_handler[0][1].state.shape = WavShape_t::Square;
	out_handler[0][1].state.max_range = 0xffff;
	out_handler[0][1].state.min_range = 0;
	out_handler[0][1].state.direction = 1;
	out_handler[0][1].state.freq_current = FREQS.f1Hz; // 0x0040 << 16;
	out_handler[0][2].set_type(GOType_t::LFO);
	out_handler[0][2].state.shape = WavShape_t::Triangle;
	out_handler[0][2].state.max_range = 0xffff;
	out_handler[0][2].state.min_range = 0;
	out_handler[0][2].state.direction = -1;
	out_handler[0][2].state.freq_current = FREQS.midi[69]; // 0x0020 << 16;
	
	key_handler.set_key_channel(1);
	key_handler.set_bend_range(4);
	
	out_handler[3][0].set_type(GOType_t::LFO);
	out_handler[3][0].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[3][0].state.gen_source.channel = 0;
	out_handler[3][0].state.shape = WavShape_t::Triangle;
	out_handler[3][0].state.direction = -1;
	out_handler[3][0].state.max_range = 0xffff;
	out_handler[3][0].state.min_range = 0;
	out_handler[3][1].set_type(GOType_t::Gate);
	out_handler[3][1].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[3][1].state.gen_source.channel = 0;
	out_handler[3][1].state.max_range = 0xffff;
	out_handler[3][1].state.min_range = 0;
	out_handler[3][2].set_type(GOType_t::Envelope);
	out_handler[3][2].state.env_num = 0;
	out_handler[3][2].state.max_range = 0xffff;
	out_handler[3][2].state.min_range = 0;
	out_handler[3][2].state.envelope_stage = 0;
	out_handler[3][2].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[3][2].state.gen_source.channel = 0;
	out_handler[3][3].set_type(GOType_t::CLK);
	out_handler[3][3].state.max_range = 0xffff;
	out_handler[3][3].state.min_range = 0;
	out_handler[3][3].state.freq_current = 23;
	out_handler[2][0].set_type(GOType_t::LFO);
	out_handler[2][0].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[2][0].state.gen_source.channel = 0;
	out_handler[2][0].state.shape = WavShape_t::Triangle;
	out_handler[2][0].state.direction = -1;
	out_handler[2][0].state.max_range = 0xffff;
	out_handler[2][0].state.min_range = 0;
	out_handler[2][1].set_type(GOType_t::Gate);
	out_handler[2][1].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[2][1].state.gen_source.channel = 0;
	out_handler[2][1].state.max_range = 0xffff;
	out_handler[2][1].state.min_range = 0;
	out_handler[2][2].set_type(GOType_t::Envelope);
	out_handler[2][2].state.env_num = 0;
	out_handler[2][2].state.max_range = 0xffff;
	out_handler[2][2].state.min_range = 0;
	out_handler[2][2].state.envelope_stage = 0;
	out_handler[2][2].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[2][2].state.gen_source.channel = 0;
	out_handler[1][3].set_type(GOType_t::DC);
	out_handler[1][3].state.gen_source.sourceType = ctrlType_t::controller;
	out_handler[1][3].state.gen_source.channel = 0;
	out_handler[1][3].state.gen_source.sourceNum = 20;
	out_handler[1][3].state.max_range = 0xffff;
	out_handler[1][3].state.currentOut = 0xffff;
	out_handler[1][3].state.min_range = 0;
	
	envelopes[0].att_current = 0x3000'0000;
	envelopes[0].att_max = 0x3000'0000;
	envelopes[0].att_min = 255;
	envelopes[0].att_source.sourceType = ctrlType_t::controller;
	envelopes[0].att_source.channel = 0;
	envelopes[0].att_source.sourceNum = 24;
	envelopes[0].dec_current = 0x0080'0000;
	envelopes[0].dec_max = 0x0800'0000;
	envelopes[0].dec_min = 0x0000'8000;
	envelopes[0].dec_source.sourceType = ctrlType_t::controller;
	envelopes[0].dec_source.channel = 0;
	envelopes[0].dec_source.sourceNum = 25;
	envelopes[0].sus_current = 0xA000;
	envelopes[0].sus_max = 0xffff;
	envelopes[0].sus_min = 0;
	envelopes[0].sus_source.sourceType = ctrlType_t::controller;
	envelopes[0].sus_source.channel = 0;
	envelopes[0].sus_source.sourceNum = 26;
	envelopes[0].rel_current = 0x1000'0000;
	envelopes[0].rel_max = 0x3fff'ffff;
	envelopes[0].rel_min = 1;
	envelopes[0].rel_source.sourceType = ctrlType_t::controller;
	envelopes[0].rel_source.channel = 0;
	envelopes[0].rel_source.sourceNum = 27;
	hasCC[4][0] = 1;
	
	// TODO: Load setup from NVM
	
	
	// Fill out utility variables
	Scan_Matrix();
	
}

void generic_output_c::update(){
	current_handler->update(&state);
}

void generic_output_c::handle_realtime(umpGeneric* msg){
	current_handler->handle_realtime(&state, msg);
}

void generic_output_c::handle_cvm(umpCVM* msg){
	current_handler->handle_cvm(&state, msg);
}

void generic_output_c::set_type(GOType_t type){
	if (state.type == type) return;
	state.type = type;
	switch (type){
	case GOType_t::DC:
		current_handler = &dc_handler;
		break;
	case GOType_t::LFO:
		current_handler = &lfo_handler;
		break;
	case GOType_t::Envelope:
		current_handler = &envelope_handler;
		break;
	case GOType_t::CLK:
		current_handler = &clk_handler;
		//state.freq_current=12; 
		break;
	case GOType_t::Pressure:
		current_handler = &pressure_handler;
		break;
	case GOType_t::Velocity:
		current_handler = &velocity_handler;
		break;
	case GOType_t::Gate:
		current_handler = &gate_handler;
		break;
	default:
		current_handler = &dc_handler;
		break;
	}
}

void lfo_output_c::update(GenOut_t* go){
	if (go->shape == WavShape_t::Sawtooth){
		go->outCount -= go->freq_current;
		go->currentOut = Rescale_16bit(go->outCount >> 16, go->min_range, go->max_range);
	} else if (go->shape == WavShape_t::SinSaw){
		go->outCount -= go->freq_current;
		go->currentOut = Rescale_16bit(TriSine(go->outCount >> 16), go->min_range, go->max_range);
	} else if (go->shape == WavShape_t::Square){
		go->outCount -= go->freq_current*2;
		uint32_t remain = go->outCount;
		if (remain < (go->freq_current*2)){
			if (go->currentOut == go->min_range){
				go->currentOut = go->max_range;
			} else {
				go->currentOut = go->min_range;
			}
		}
	} else {
		if (go->shape == WavShape_t::Sine){
			go->currentOut = Rescale_16bit(TriSine(go->outCount >> 16), go->min_range, go->max_range);
		} else {
			go->currentOut = Rescale_16bit(go->outCount >> 16, go->min_range, go->max_range);
		}
		
		if (go->direction == 1){
			uint32_t remain = (0xFFFF'FFFF << 16) - go->outCount;
			if (remain <= (2*go->freq_current)){
				// change direction
				go->direction = -1;
				uint32_t diff = (2*go->freq_current) - remain;
				go->outCount = (0xFFFF'FFFF << 16) - diff;
			} else {
				go->outCount += (2*go->freq_current);
			}
		} else {
			uint32_t remain = go->outCount;
			if (remain <= (2*go->freq_current)){
				go->direction = 1;
				uint32_t diff = (2*go->freq_current) - remain;
				go->outCount = diff;
			} else {
				go->outCount -= (2*go->freq_current);
			}
		}
	}
}

void envelope_output_c::update(GenOut_t* go){
	Env_t* tempEnv = &envelopes[go->env_num];
	uint32_t remain;
	switch(go->envelope_stage){
		case 1:
			// attack
			remain = (0xFFFF'FFFF << 16) - go->outCount;
			if (remain <= tempEnv->att_current){
				go->envelope_stage++;
				go->outCount = 0xFFFF'FFFF << 16;
			} else {
				go->outCount += tempEnv->att_current;
			}
			break;
		case 2:
			// decay
			remain = go->outCount - (tempEnv->sus_current << 16);
			if (remain <= tempEnv->dec_current){
				go->envelope_stage++;
				go->outCount = tempEnv->sus_current << 16;
			} else {
				go->outCount -= tempEnv->dec_current;
			}
			break;
		case 4:
			// release
			if (go->outCount <= tempEnv->rel_current){
				go->envelope_stage = 0;
				go->outCount = 0;
			} else {
				go->outCount -= tempEnv->rel_current;
			}
			break;
		default:
			break;
	}
	// Set new value
	go->currentOut = Rescale_16bit(go->outCount >> 16, go->min_range, go->max_range);
}

// TODO: handle RPN for poly pitch bend
// TODO: handle note on attribute data for microtonal
void dc_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	uint32_t src_current;
	uint32_t criteria;
	switch (msg->status){
	case NOTE_ON:
		genout->gen_source.sourceNum = msg->note;
		genout->currentOut = Note_To_Output(msg->note);
		// Expect caller to check channel etc
		return;
	case NOTE_OFF:
		// Expect caller to check channel etc
		return;
	case CC:
	case NRPN:
		criteria = ( msg->channel << 0 ) | ( msg->index << 8 ) | ( uint8_t(ctrlType_t::controller) << 24);
		src_current = ( genout->gen_source.channel << 0 ) | ( genout->gen_source.sourceNum << 8 ) | ( uint8_t(genout->gen_source.sourceType) << 24 );
		if ( src_current == criteria ){
			// CV generation
			uint32_t span = (genout->max_range - genout->min_range) + 1;
			uint32_t scaled = (msg->value >> 16) * span;
			genout->currentOut = (scaled >> 16) + genout->min_range;
		}
		break;
	case PROGRAM_CHANGE:
		criteria = ( msg->channel << 0 ) | ( msg->index << 8 ) | ( uint8_t(ctrlType_t::program) << 24);
		src_current = ( genout->gen_source.channel << 0 ) | ( genout->gen_source.sourceNum << 8 ) | ( uint8_t(genout->gen_source.sourceType) << 24 );
		if ( src_current == criteria ){
			if (genout->currentOut == genout->max_range){
				genout->currentOut = genout->min_range;
			} else {
				genout->currentOut = genout->max_range;
			}
			return;
		}
		break;
	case PITCH_BEND:
		// update v/oct outputs
		criteria = ( key_handler.get_key_channel() << 0 ) | ( uint8_t(ctrlType_t::key) << 8 );
		src_current = ( genout->gen_source.channel << 0 ) | ( uint8_t(genout->gen_source.sourceType) << 8 );
		if ( src_current == criteria ){
			genout->currentOut = Note_To_Output(genout->gen_source.sourceNum);
		}
		break;
	default:
		return;
	}
}

// TODO: use CC/NRPN lookup table
// TODO: handle RPN for poly pitch bend
// TODO: handle note on attribute data for microtonal
void lfo_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	uint32_t criteria;
	uint32_t src_current;
	if (genout->gen_source.sourceType == ctrlType_t::controller){
		if (msg->status == CC){
			criteria = ( msg->channel << 0 ) | ( msg->index << 8 );
			src_current = (genout->gen_source.channel << 0) | (genout->gen_source.sourceNum << 8);
		} else if (msg->status == NRPN){	// TODO: per-note CC
			criteria = ( msg->channel << 0 ) | ( msg->index << 8 );	// TODO: handle banks?
			src_current = (genout->gen_source.channel << 0) | (genout->gen_source.sourceNum << 8);
		} else {
			return;
		}	
		if ( src_current == criteria ){
			// Wave generation
			uint64_t span = genout->freq_max - genout->freq_min + 1;
			uint64_t scaled = msg->value * span;
			genout->freq_current = (scaled >> 32) + genout->freq_min;
		}	
	} else if (genout->gen_source.sourceType == ctrlType_t::key){
		uint32_t criteria = ( key_handler.get_key_channel() << 0 ) | ( uint8_t(ctrlType_t::key) << 8 );
		uint32_t src_current = ( genout->gen_source.channel << 0 ) | ( uint8_t(genout->gen_source.sourceType) << 8 );
		if (src_current != criteria ) {
			return;
		}
		// TODO: allow detuning and sub-audible oscillators
		// TODO: handle range limiting
		// TODO: exponential bending
		if (msg->status == NOTE_ON){
			//genout->freq_current = FREQS.midi[msg->note];
			genout->gen_source.sourceNum = msg->note;
		} else if (msg->status == PITCH_BEND){
			// Bend must be taken account of at note start too
		}
		int64_t tempBend = key_handler.get_current_bend();
		tempBend *= FREQS.midi[genout->gen_source.sourceNum];
		tempBend *= FIXED_VOLT_PER_INT;
		if (tempBend < 0) {
			tempBend >>= 1;	// Bending down one octave should halve the frequency
		}
		tempBend >>= FIXED_POINT_POS;
		int32_t bendfreq = tempBend;
		genout->freq_current = FREQS.midi[genout->gen_source.sourceNum] + bendfreq;
	}
}

void envelope_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	if (msg->status == NOTE_ON) {
		genout->gen_source.sourceNum = msg->note;
		//genout->outCount = tempOut->min_range << 16;
		genout->envelope_stage = 1;
	} else if (msg->status == NOTE_OFF) {
		genout->envelope_stage = 4;
	}
}

// TODO: fix keylanes, maybe split handle cc from handle note and add a subscriber mechanism?
// TODO: determine if channel or poly pressure is used (msg->note is only needed for poly pressure)
// TODO: should pressure outputs always react to velocity?
void pressure_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	if (msg->status == NOTE_ON){
	} else if (msg->status == CHANNEL_PRESSURE){
	} else if (msg->status == KEY_PRESSURE){
	} else {
		return;
	}
	genout->gen_source.sourceNum = msg->note;
	genout->currentOut = Rescale_16bit(msg->value >> 16, genout->min_range, genout->max_range);
}

void velocity_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	if (msg->status == NOTE_ON){
		genout->gen_source.sourceNum = msg->note;
		genout->currentOut = Rescale_16bit(msg->value, genout->min_range, genout->max_range);
	} else if (msg->status == NOTE_OFF){
		genout->currentOut = Rescale_16bit(msg->value, genout->min_range, genout->max_range);
	}
}

void gate_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	if (msg->status == NOTE_ON){
		genout->gen_source.sourceNum = msg->note;
		genout->currentOut = genout->max_range;
	} else if (msg->status == NOTE_OFF){
		genout->currentOut = genout->min_range;
	}
}

void key_handler_c::reset(){
	next_lane = 0;
	queue_index = 0;
	channel = 1;
	num_lanes = 0;
	num_coms = 0;
	for (uint8_t i = 0; i < 8; i++){
		num_outputs[i] = 0;
		key_playing[i] = -1;
		drum_note[i] = -1;
	}
}

void key_handler_c::stop_notes(){
	queue_index = 0;
	for(uint8_t l = 0; l < num_lanes; l++){
		key_playing[l] = -1;
		for (uint8_t i = 0; i < num_outputs[l]; i++){
			GenOut_t* temp_out = &lanes[l][i]->state;
			if (temp_out->type == GOType_t::Envelope){
				temp_out->currentOut = temp_out->min_range;
				temp_out->envelope_stage = 4;
			} else if (temp_out->type == GOType_t::Gate){
				temp_out->currentOut = temp_out->min_range;
			}
		}
	}
}

void key_handler_c::start_note(uint8_t lane, umpCVM* msg){
	for (uint8_t i = 0; i < num_outputs[lane]; i++){
		lanes[lane][i]->handle_cvm(msg);
	}
	
	key_playing[lane] = msg->note;
	
	// Start common outputs
	start_note(msg);
}


void key_handler_c::start_note(umpCVM* msg){
	// Handle shared outputs
	for (uint8_t i = 0; i < num_coms; i++){
		com_out[i]->handle_cvm(msg);
	}
}

void key_handler_c::stop_note(uint8_t lane, umpCVM* msg){
	for (uint8_t i = 0; i < num_outputs[lane]; i++){
		lanes[lane][i]->handle_cvm(msg);
	}

	key_playing[lane] = -1;
	
	// Check lanestates, to know if shared outputs should be turned off
	bool foundActive = false;
	for (uint8_t j = 0; j < num_lanes; j++){
		if (key_playing[j] >= 0){
			foundActive = true;
			break;
		}
	}
	if (foundActive){
		for (uint8_t j = 0; j < num_coms; j++){
			if ( com_out[j]->state.type == GOType_t::Envelope ){
				// Re-trigger envelope
				com_out[j]->state.envelope_stage = 1;
			}
		}
	} else {
		stop_note(msg);
	}
}
	
void key_handler_c::stop_note(umpCVM* msg){
	// Handle shared outputs
	for (uint8_t j = 0; j < num_coms; j++){
		com_out[j]->handle_cvm(msg);
	}
}

void key_handler_c::set_bend_range(uint8_t range){
	// Configure note bend range
	max_bend = 0x7fff + (uint32_t)(INT_PER_NOTE * (range+1));
	min_bend = 0x7fff - (uint32_t)(INT_PER_NOTE * (range+1));
}

uint8_t key_handler_c::handle_cvm(umpCVM* msg){
	if (msg->status == NOTE_ON){
		if (msg->channel == 9){
			// Drum channel
			for (int8_t l = 7; l >= num_lanes; l--){
					if ( msg->note == drum_note[l] ){
						key_playing[l] = drum_note[l];
						for (uint8_t i = 0; i < num_outputs[l]; i++){
							lanes[l][i]->handle_cvm(msg);
						}
						break;
					} 
			}
		} else if(channel == msg->channel) {
			if (num_lanes == 0) {
				// Unison mode
				if (key_playing[0] >= 0) {
					// Note already playing in lane. Push to queue
					note_queue[queue_index++].note = key_playing[0];
				}
				start_note(0, msg);
				return 1;
			}
			// Polyphonic mode
			uint8_t tempLane = 0x80 | next_lane;
			for (uint8_t l = 0; l < num_lanes; l++){
				uint8_t lane = next_lane + l;
				if (lane >= num_lanes) {
					lane -= num_lanes;
				}
				if (key_playing[lane] == msg->note){
					// Note is already playing
					return 1;
				}
				if (tempLane & 0x80){
					if (key_playing[lane] < 0){
						// Found an unused lane
						tempLane = lane;
						//break;
					}
				}
			}
			
			tempLane &= 0x7f;
				
			// Update starting lane for Round-robin arbitration
			next_lane = tempLane + 1;
			if (num_lanes == 0){
				next_lane = 0;
			} else if (next_lane >= num_lanes) {
				next_lane -= num_lanes;
			}
				
			if (key_playing[tempLane] >= 0) {
				// Note already playing in lane. Push to queue
				note_queue[queue_index++].note = key_playing[tempLane];
			}
			start_note(tempLane, msg);
		}	
		return 1;
	} else if (msg->status == NOTE_OFF){
		if (msg->channel == 9){
			// Drum channel
			for (int8_t l = 7; l >= num_lanes; l--){
					if ( msg->note == drum_note[l] ){
						key_playing[l] = drum_note[l];
						for (uint8_t i = 0; i < num_outputs[l]; i++){
							lanes[l][i]->handle_cvm(msg);
						}
						break;
					} 
			}
		} else if(channel == msg->channel) {
			// Look for note in queue
			uint8_t i;
			for (i = 0; i < queue_index; i++){
				if (note_queue[i].note == msg->note){
					queue_index--;
					break;
				}
			}
			// Overwrite that note
			for (; i < queue_index; i++){
				note_queue[i] = note_queue[i+1];
			}
				
			if (num_lanes == 0){
				// Unison mode
				if (key_playing[0] != msg->note) return 1;
				if (queue_index > 0){
					// Start last note which was put in queue
					umpCVM tempMsg;
					tempMsg.status = NOTE_ON;
					tempMsg.note = note_queue[--queue_index].note;
					tempMsg.value = msg->value;
					start_note(0,&tempMsg);
				} else {
					stop_note(0,msg);
				}
				return 1;
			}
			// Polyphonic mode
			// Find the used lane
			uint8_t tempLane = 0x80;
			for (uint8_t l = 0; l < num_lanes; l++){
				if (key_playing[l] == msg->note){
					tempLane = l;
					break;
				}
			}
				
			if (tempLane != 0x80){
				if (queue_index > 0){
					// Start last note which was put in queue
					umpCVM tempMsg;
					tempMsg.status = NOTE_ON;
					tempMsg.note = note_queue[--queue_index].note;
					tempMsg.value = msg->value;
					start_note(tempLane, &tempMsg);
				} else {
					// Stop note
					stop_note(tempLane, msg);
				}
			}
		}	
		return 1;
	} else if (msg->status == PITCH_BEND){
		// TODO: keychannel
		uint16_t tempBend = Rescale_16bit(msg->value >> 16, min_bend, max_bend);
		current_bend = tempBend - 0x7fff;
		for (uint8_t l = 0; l < num_lanes; l++){
			if (key_playing[l] < 0){
				// Not playing
				continue;
			}
			for (uint8_t i = 0; i < num_outputs[l]; i++){
				lanes[l][i]->handle_cvm(msg);
			}
		}
		for (uint8_t i = 0; i < num_coms; i++){
			com_out[i]->handle_cvm(msg);
		}
		return 1;
	}	
	// TODO: pressure
	// TODO: combined pressure and velocity
	return 0;
}

uint8_t key_handler_c::subscribe_key(generic_output_c* handler){
	return subscribe_key(handler, handler->get_key_lane());
}

uint8_t key_handler_c::subscribe_key(generic_output_c* handler, uint8_t lane){
	if (lane == 0){
		com_out[num_coms++] = handler;
	} else {
		if (num_outputs[lane] >= 4){
			return 0;
		}
		lanes[lane][num_outputs[lane]++] = handler;
		if (num_lanes < lane){
			num_lanes = lane;
		}
	}
	return 1;
}

uint8_t key_handler_c::subscribe_drum(generic_output_c* handler){
	// TODO:
}

// TODO: Fix stuck notes
// TODO: Optimize pitchbend
void GO_MIDI_Voice(struct umpCVM* msg){
	if(msg->umpGroup != midi_group){
		return;
	}
	
	// Handle special messages
	if (msg->status == CC){
		if ((msg->index == 120)||(msg->index == 123)){
			key_handler.stop_notes();
			return;
		} else if (msg->index == 121){
			Reset_All_Controllers();
			return;
		}
	}
	
	// Handle note messages
	if (key_handler.handle_cvm(msg)) {
		return;
	}

	// Handle outputs
	for (uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			out_handler[x][y].handle_cvm(msg);
		}
	}

	// TODO: refactor envelope handling
	if (msg->status == CC){
		// TODO: controlnum -> control lookup table
		uint16_t controlNum = msg->index;
		// Search envelopes
		uint32_t criteria = ( uint8_t(ctrlType_t::controller) << 0 ) | ( msg->channel << 8 ) | ( controlNum << 16 );
		for (uint8_t i = 0; i < 4; i++){
			if (!hasCC[4][i]){
				continue;
			}
			
			// TODO: fix scaling
			// Attack
			uint32_t src_current = 
				( uint8_t(envelopes[i].att_source.sourceType) << 0 ) | 
				( envelopes[i].att_source.channel << 8 ) | 
				( envelopes[i].att_source.sourceNum << 16 );
			if (src_current == criteria){
				if (envelopes[i].att_max > envelopes[i].att_min){
					uint32_t diff = envelopes[i].att_max - envelopes[i].att_min;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].att_current = 0x0101 * envelopes[i].att_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].att_min - envelopes[i].att_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].att_current = 0x0101 * envelopes[i].att_min - (scaled >> 16);
				}
			}
			
			// Decay
			src_current =
				( uint8_t(envelopes[i].dec_source.sourceType) << 0 ) |
				( envelopes[i].dec_source.channel << 8 ) |
				( envelopes[i].dec_source.sourceNum << 16 );
			if ( src_current == criteria ){
				if (envelopes[i].dec_max > envelopes[i].dec_min){
					uint32_t diff = envelopes[i].dec_max - envelopes[i].dec_min;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].dec_current = 0x0101 * envelopes[i].dec_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].dec_min - envelopes[i].dec_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].dec_current = 0x0101 * envelopes[i].dec_min - (scaled >> 16);
				}	
			}
			
			// Sustain
			src_current =
				( uint8_t(envelopes[i].sus_source.sourceType) << 0 ) |
				( envelopes[i].sus_source.channel << 8 ) |
				( envelopes[i].sus_source.sourceNum << 16 );
			if ( src_current == criteria ){
				if (envelopes[i].sus_max > envelopes[i].sus_min){
					uint32_t diff = envelopes[i].sus_max - envelopes[i].sus_min;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].sus_current = 0x0101 * envelopes[i].sus_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].sus_min - envelopes[i].sus_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].sus_current = 0x0101 * envelopes[i].sus_min - (scaled >> 16);
				}
			}
			
			// Release
			src_current =
				( uint8_t(envelopes[i].rel_source.sourceType) << 0 ) |
				( envelopes[i].rel_source.channel << 8 ) |
				( envelopes[i].rel_source.sourceNum << 16 );
			if ( src_current == criteria ){
				if (envelopes[i].rel_max > envelopes[i].rel_min){
					uint32_t diff = envelopes[i].rel_max - envelopes[i].rel_min;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].rel_current = 0x0101 * envelopes[i].rel_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].rel_min - envelopes[i].rel_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->value >> 16) * span;
					envelopes[i].rel_current = 0x0101 * envelopes[i].rel_min - (scaled >> 16);
				}				
			}
		}
	}
	return;
}

void clk_output_c::handle_realtime(GenOut_t* genout, umpGeneric* msg){
	if(msg->status == TIMINGCLOCK){
		genout->outCount++;
		if (genout->outCount > genout->freq_current){
			genout->outCount = 0;
			genout->currentOut = (genout->currentOut == genout->max_range) ? genout->min_range : genout->max_range;
		}
	}
}

void GO_MIDI_Realtime(struct umpGeneric* msg){
		for (uint8_t x = 0; x < 4; x++){
			for (uint8_t y = 0; y < 4; y++){
				out_handler[x][y].handle_realtime(msg);
			}
		}
}

void GO_Service(){
	// Update configuration when needed
	if (needScan) Scan_Matrix();
	
	for (uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			out_handler[x][y].update();
		}
	}
}

void GO_Service(uint8_t x){
	// Update configuration when needed
	if (needScan) Scan_Matrix();
	
	for (uint8_t y = 0; y < 4; y++){
		out_handler[x][y].update();
	}
}

// Return sine output from linear input
uint16_t base_output_c::TriSine(uint16_t in){
	// Smoothstep approximation
	// In**1	
	uint32_t tempIn = in;
	int32_t tempOut = 0;
	// In**2
	tempIn *= in;
	tempIn >>= 16;
	// In**3
	tempIn *= in;
	tempIn >>= 16;
	tempOut += 10*tempIn;
	// In**4
	tempIn *= in;
	tempIn >>= 16;
	tempOut -= 15*tempIn;
	// In**5
	tempIn *= in;
	tempIn >>= 16;
	tempOut += 6*tempIn;
	
	// Clamp output
	if (tempOut > 0xFFFF){
		tempOut = 0xFFFF;
	} else if (tempOut < 0) {
		tempOut = 0;
	}
	
	return tempOut;
}

