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

bool needScan = false;

Env_t envelopes[4];

dc_output_c generic_output_c::dc_handler = dc_output_c();
lfo_output_c generic_output_c::lfo_handler = lfo_output_c();
envelope_output_c generic_output_c::envelope_handler = envelope_output_c();
clk_output_c generic_output_c::clk_handler = clk_output_c();
pressure_output_c generic_output_c::pressure_handler = pressure_output_c();
velocity_output_c generic_output_c::velocity_handler = velocity_output_c();
gate_output_c generic_output_c::gate_handler = gate_output_c();

generic_output_c out_handler[4][4];
dc_output_c 		dc_handler = dc_output_c();
lfo_output_c 		lfo_handler = lfo_output_c();
envelope_output_c 	envelope_handler = envelope_output_c();
clk_output_c 		clk_handler = clk_output_c();
pressure_output_c 	pressure_handler = pressure_output_c();
velocity_output_c 	velocity_handler = velocity_output_c();
gate_output_c 		gate_handler = gate_output_c();

struct keyLanes_t {
	uint8_t note;
	enum keyState_t {
		KeyNone = 0,
		KeyIdle = 1,
		KeyPlaying} state;
} keyLanes[4];
uint8_t currentKeyLane = 0;
uint8_t keyChannel = 1;
GridPos_t keyPara[8];
uint8_t keyParaNum;
uint16_t keyMask;

uint8_t bendRange = 4;
int16_t currentBend = 0;
uint16_t maxBend;
uint16_t minBend;

uint8_t queueIndex;
struct {
	uint8_t note;
} noteQueue[32];

// hasCC[4] is for envelopes
bool hasCC[5][4];

uint8_t midi_group = 1;

#define ENV_MANTISSA 7

// TODO: update gain if switching to DACs
#define OUTPUT_GAIN 1.0453		// Nominal gain
#define INT_PER_VOLT 6553.6/OUTPUT_GAIN
#define INT_PER_NOTE INT_PER_VOLT/12
#define FIXED_POINT_POS 14
#define FIXED_INT_PER_NOTE ((uint32_t) (INT_PER_NOTE * (1 << FIXED_POINT_POS)))

// TODO: Setting gate on y > 1 breaks lanes. Sometimes???
// Scan the configuration
// To populate time saving variables
void Scan_Matrix(){
	// find Keychannel
	bool foundChannel = false;
	for(uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::key){
				if (out_handler[x][y].state.gen_source.channel != 9){
					keyChannel = out_handler[x][y].state.gen_source.channel;
					foundChannel = true;
					break;
				}
			}
		}
		if (foundChannel){
			break;
		}
	}
	
	// Scan matrix
	int32_t lane_conf[4];
	for(uint8_t x = 0; x < 4; x++){
		lane_conf[x] = 0;
		for (uint8_t y = 0; y < 4; y++){
			// Get Keylane configuration
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::key){
				if (out_handler[x][y].state.gen_source.channel == keyChannel){
					lane_conf[x] |= 1 << (4 * ((uint8_t) out_handler[x][y].state.type) + y);
				}
			}
		}
	}
	
	uint8_t maxCom = 0;
	uint32_t comConf = 0;
	for (uint8_t i = 0; i < 4; i++){
		// Detect lane similarities
		if (lane_conf[i]){
			for (uint8_t j = 0; j < i; j++){
				if (lane_conf[j]){
					uint8_t numCom = 0;
					uint32_t conf = lane_conf[i] & lane_conf[j];
					for (uint8_t k = 0; k < 32; k++){
						numCom += (conf >> k) & 1;	
					}
					if (numCom > maxCom){
						maxCom = numCom;
						comConf = conf;
					}
				}
			}
			// Set config for fallback
			if (!comConf){
				comConf = lane_conf[i];
			}
		}
	}
	
	// Find common outputs
	keyParaNum = 0;
	keyMask = 0;
	uint16_t matKey = (uint16_t) ctrlType_t::key | (keyChannel << 8);
	for (uint8_t x = 0; x < 4; x++){
		uint8_t hasLane = 0;
		// Has key outputs?
		if (lane_conf[x]){
			for (uint8_t y = 0; y < 4; y++){
				if (comConf & (1 << (4 * ((uint8_t) out_handler[x][y].state.type) + y))){
					// Not a common output?
					if ((comConf & lane_conf[x]) == comConf){
						// Not a common output!
						keyMask |= 1 << (y + 4*x);
						hasLane = 1;
						continue;
					}
				}
				uint16_t tempKey = (uint16_t) out_handler[x][y].state.gen_source.sourceType | (out_handler[x][y].state.gen_source.channel << 8);
				if (tempKey == matKey){
					// Common output
					keyPara[keyParaNum].x = x;
					keyPara[keyParaNum].y = y;
					keyParaNum++;
				}
			}
			// Update keylane states
			if (!((uint8_t) keyLanes[x].state) != !hasLane){
				keyLanes[x].state = (keyLanes_t::keyState_t) hasLane;
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
	
	// Configure note bend range
	maxBend = 0x7fff + (uint32_t)((INT_PER_VOLT/8) * bendRange);
	minBend = 0x7fff - (uint32_t)((INT_PER_VOLT/8) * bendRange);
	
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
	tempOut += currentBend;
	
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
	} else if (minOut > maxOut) {
		tempResult = (minOut-maxOut+1) * val;
		tempResult >>= 16; // Divide by input range
		tempResult = minOut - tempResult;
	}
	return (uint16_t) tempResult;
}

inline void Start_Note(uint8_t lane, uint8_t note, uint16_t velocity){
	for (uint8_t y = 0; y < 4; y++){
		if (!(keyMask & (1 << (y + 4*lane)))){
			continue;
		}
		GenOut_t* tempOut = &out_handler[lane][y].state;
		
		if ( tempOut->type == GOType_t::DC ){
			tempOut->gen_source.sourceNum = note;
			tempOut->currentOut = Note_To_Output(note);
			continue;
		}
		
		if ( tempOut->type == GOType_t::Gate ){
			tempOut->gen_source.sourceNum = note;
			tempOut->currentOut = tempOut->max_range;
			continue;
		}
		
		if ( tempOut->type == GOType_t::Velocity ){
			tempOut->gen_source.sourceNum = note;
			tempOut->currentOut = Rescale_16bit(velocity, tempOut->min_range, tempOut->max_range);
			continue;
		}
		
		if ( tempOut->type == GOType_t::Envelope ){
			tempOut->gen_source.sourceNum = note;
			//tempOut->outCount = tempOut->min_range << 16;
			tempOut->envelope_stage = 1;
			continue;
		}
	}
	
	keyLanes[lane].state = keyLanes_t::KeyPlaying;
	keyLanes[lane].note = note;
	
	// Handle shared outputs
	for (uint8_t i = 0; i < keyParaNum; i++){
		GenOut_t* tempOut = &out_handler[keyPara[i].x][keyPara[i].y].state;
		
		if ( tempOut->type == GOType_t::DC ){
			tempOut->gen_source.sourceNum = note;
			tempOut->currentOut = Note_To_Output(note);
			continue;
		}
		
		if ( tempOut->type == GOType_t::Gate ){
			tempOut->gen_source.sourceNum = note;
			tempOut->currentOut = tempOut->max_range;
			continue;
		}
		
		if ( tempOut->type == GOType_t::Velocity ){
			tempOut->gen_source.sourceNum = note;
			tempOut->currentOut = Rescale_16bit(velocity, tempOut->min_range, tempOut->max_range);
			continue;
		}
		
		if ( tempOut->type == GOType_t::Envelope ){
			tempOut->gen_source.sourceNum = note;
			//tempOut->outCount = tempOut->min_range << 16;
			tempOut->envelope_stage = 1;
			continue;
		}
		
	}
}

inline void Stop_Note(uint8_t lane){
	for (uint8_t y = 0; y < 4; y++){
		if (!(keyMask & (1 << (y + 4*lane)))){
			continue;
		}
		
		if ( out_handler[lane][y].state.type == GOType_t::Gate ){
			out_handler[lane][y].state.currentOut = out_handler[lane][y].state.min_range;
			continue;
		} 
		
		if ( out_handler[lane][y].state.type == GOType_t::Envelope ){
			out_handler[lane][y].state.envelope_stage = 4;
			continue;
		}
	}
	
	keyLanes[lane].state = keyLanes_t::KeyIdle;
	
	// Check lanestates, to know if shared outputs should be turned off
	bool foundActive = false;
	for (uint8_t i = 0; i < 4; i++){
		if (keyLanes[i].state == keyLanes_t::KeyPlaying){
			foundActive = true;
			break;
		}
	}
	
	// Handle shared outputs
	for (uint8_t i = 0; i < keyParaNum; i++){
		GenOut_t* tempOut = &out_handler[keyPara[i].x][keyPara[i].y].state;
				
		if ( tempOut->type == GOType_t::Gate ){
			if (!foundActive){
				tempOut->currentOut = tempOut->min_range;
			}
			continue;
		}
		
		if ( tempOut->type == GOType_t::Envelope ){
			if (foundActive){
				tempOut->envelope_stage = 1;
			} else {
				tempOut->envelope_stage = 4;
			}
			continue;
		}
		
	}
	
}

inline void Stop_All_Notes(){
	queueIndex = 0;
	for(uint8_t x = 0; x < 4; x++){
		if (keyLanes[x].state == keyLanes_t::KeyPlaying){
			keyLanes[x].state = keyLanes_t::KeyIdle;
		}
		for (uint8_t y = 0; y < 4; y++){
			GenOut_t* tempOut = &out_handler[x][y].state;
			if (tempOut->type == GOType_t::Envelope){
				tempOut->currentOut = tempOut->min_range;
				tempOut->envelope_stage = 4;
			} else if (tempOut->type == GOType_t::Gate){
				tempOut->currentOut = tempOut->min_range;
			}
		}
	}
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
	out_handler[0][3].state.type = GOType_t::LFO;
	out_handler[0][3].state.shape = WavShape_t::Sawtooth;
	out_handler[0][3].state.max_range = 0x3fff;
	out_handler[0][3].state.min_range = 0;
	out_handler[0][3].state.direction = -1;
	out_handler[0][3].state.freq_current = 0x0008 << 16;
	out_handler[1][2].state.type = GOType_t::LFO;
	out_handler[1][2].state.shape = WavShape_t::Sawtooth;
	out_handler[1][2].state.max_range = 0xffff;
	out_handler[1][2].state.min_range = 0x3fff;
	out_handler[1][2].state.direction = -1;
	out_handler[1][2].state.freq_current = 0x0010 << 16;
	out_handler[1][2].state.freq_max = 0x01000000;
	out_handler[1][2].state.freq_min = 0x00001000;
	out_handler[1][2].state.gen_source.channel = 1;
	out_handler[1][2].state.gen_source.sourceNum = 10;
	out_handler[1][2].state.gen_source.sourceType = ctrlType_t::controller;
	hasCC[1][2] = 1;
	
	out_handler[0][1].state.type = GOType_t::LFO;
	out_handler[0][1].state.shape = WavShape_t::Square;
	out_handler[0][1].state.max_range = 0xffff;
	out_handler[0][1].state.min_range = 0;
	out_handler[0][1].state.direction = 1;
	out_handler[0][1].state.freq_current = 0x0040 << 16;
	out_handler[0][2].state.type = GOType_t::LFO;
	out_handler[0][2].state.shape = WavShape_t::Triangle;
	out_handler[0][2].state.max_range = 0xffff;
	out_handler[0][2].state.min_range = 0;
	out_handler[0][2].state.direction = -1;
	out_handler[0][2].state.freq_current = 0x0020 << 16;
	
	keyChannel = 1;
	keyLanes[0].state = keyLanes_t::KeyNone;
	keyLanes[1].state = keyLanes_t::KeyNone;
	keyLanes[2].state = keyLanes_t::KeyNone;
	keyLanes[3].state = keyLanes_t::KeyIdle;
	
	out_handler[3][0].state.type = GOType_t::DC;
	out_handler[3][0].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[3][0].state.gen_source.channel = 0;
	out_handler[3][0].state.max_range = 0xffff;
	out_handler[3][0].state.min_range = 0;
	out_handler[3][1].state.type = GOType_t::Gate;
	out_handler[3][1].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[3][1].state.gen_source.channel = 0;
	out_handler[3][1].state.max_range = 0xffff;
	out_handler[3][1].state.min_range = 0;
	out_handler[3][2].state.type = GOType_t::Envelope;
	out_handler[3][2].state.env_num = 0;
	out_handler[3][2].state.max_range = 0xffff;
	out_handler[3][2].state.min_range = 0;
	out_handler[3][2].state.envelope_stage = 0;
	out_handler[3][2].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[3][2].state.gen_source.channel = 0;
	out_handler[3][3].state.type = GOType_t::CLK;
	out_handler[3][3].state.max_range = 0xffff;
	out_handler[3][3].state.min_range = 0;
	out_handler[3][3].state.freq_current = 23;
	out_handler[2][0].state.type = GOType_t::DC;
	out_handler[2][0].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[2][0].state.gen_source.channel = 0;
	out_handler[2][0].state.max_range = 0xffff;
	out_handler[2][0].state.min_range = 0;
	out_handler[2][1].state.type = GOType_t::Gate;
	out_handler[2][1].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[2][1].state.gen_source.channel = 0;
	out_handler[2][1].state.max_range = 0xffff;
	out_handler[2][1].state.min_range = 0;
	out_handler[2][2].state.type = GOType_t::Envelope;
	out_handler[2][2].state.env_num = 0;
	out_handler[2][2].state.max_range = 0xffff;
	out_handler[2][2].state.min_range = 0;
	out_handler[2][2].state.envelope_stage = 0;
	out_handler[2][2].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[2][2].state.gen_source.channel = 0;
	out_handler[1][3].state.type = GOType_t::DC;
	out_handler[1][3].state.gen_source.sourceType = ctrlType_t::controller;
	out_handler[1][3].state.gen_source.channel = 0;
	out_handler[1][3].state.gen_source.sourceNum = 20;
	out_handler[1][3].state.max_range = 0xffff;
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
	current_handler.update(&state);
}

void generic_output_c::handle_realtime(umpGeneric* msg){
	current_handler.handle_realtime(&state, msg);
}

void generic_output_c::handle_cvm(umpCVM* msg){
	current_handler.handle_cvm(&state, msg);
}

void generic_output_c::set_type(GOType_t type){
	state.type = type;
	switch (type){
	case GOType_t::DC:
		current_handler = dc_handler;
		break;
	case GOType_t::LFO:
		current_handler = lfo_handler;
		break;
	case GOType_t::Envelope:
		current_handler = envelope_handler;
		break;
	case GOType_t::CLK:
		current_handler = clk_handler;
		break;
	case GOType_t::Pressure:
		current_handler = pressure_handler;
		break;
	case GOType_t::Velocity:
		current_handler = velocity_handler;
		break;
	case GOType_t::Gate:
		current_handler = gate_handler;
		break;
	default:
		current_handler = dc_handler;
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
		go->outCount -= go->freq_current;
		uint32_t remain = go->outCount;
		if (remain < go->freq_current){
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
			if (remain <= go->freq_current){
				// change direction
				go->direction = -1;
				uint32_t diff = go->freq_current - remain;
				go->outCount = (0xFFFF'FFFF << 16) - diff;
			} else {
				go->outCount += go->freq_current;
			}
		} else {
			uint32_t remain = go->outCount;
			if (remain <= go->freq_current){
				go->direction = 1;
				uint32_t diff = go->freq_current - remain;
				go->outCount = diff;
			} else {
				go->outCount -= go->freq_current;
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

void dc_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	uint32_t src_current;
	uint32_t criteria;
	switch (msg->status){
	case NOTE_ON:
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
		break;
	default:
		return;
	}
}

// TODO: use CC/NRPN lookup table
void lfo_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	uint32_t criteria;
	uint32_t src_current;
	if (genout->gen_source.sourceType != ctrlType_t::controller){
		return;
	}
	if (msg->status == CC){
		criteria = ( msg->channel << 0 ) | ( msg->index << 8 );
		src_current = (genout->gen_source.channel << 0) | (genout->gen_source.sourceNum << 8);
	} else if (msg->status == NRPN){
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
}

void envelope_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	
}

// TODO: fix keylanes, maybe split handle cc from handle note and add a subscriber mechanism?
// TODO: determine if channel or poly pressure is used (msg->note is only needed for poly pressure)
void pressure_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	uint32_t criteria;
	uint32_t src_current;
	if (msg->status == CHANNEL_PRESSURE){
		criteria = ( keyChannel << 0 ) | ( uint8_t(ctrlType_t::key) << 8 );
		src_current = 
			( genout->gen_source.channel << 0 ) | 
			( uint8_t(genout->gen_source.sourceType) << 8 );
	} else if (msg->status == KEY_PRESSURE){
		criteria = ( keyChannel << 0 ) | ( uint8_t(ctrlType_t::key) << 8 );// | ( msg->note << 16 );
		src_current = 
			( genout->gen_source.channel << 0 ) | 
			( uint8_t(genout->gen_source.sourceType) << 8 );// |
			//( keyLanes[x].note << 16 );
	} else {
		return;
	}
	if ( src_current == criteria ){
		genout->currentOut = Rescale_16bit(msg->value >> 16, genout->min_range, genout->max_range);
	}
}

void velocity_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	
}

void gate_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	
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
			Stop_All_Notes();
			return;
		} else if (msg->index == 121){
			Reset_All_Controllers();
			return;
		}
	}
	
	// TODO: refactor keylanes
	// Handle note messages
	if (msg->status == NOTE_ON){
		// TODO: trigger keylanes
		if (msg->channel == 9){
			// Drum channel
			uint32_t criteria = ( msg->channel << 8 ) | ( msg->note << 16 );
			bool foundLane = false;
			for (uint8_t x = 0; x < 4; x++){
				for (uint8_t y = 0; y < 4; y++){
					uint32_t src_current = 
						( uint8_t(out_handler[x][y].state.type) << 0 ) |
						( out_handler[x][y].state.gen_source.channel << 8 ) |
						( out_handler[x][y].state.gen_source.sourceNum << 16 );
					if ( src_current == ( criteria | uint8_t(GOType_t::Envelope) ) ){
						foundLane = true;
						out_handler[x][y].state.envelope_stage = 1;
						out_handler[x][y].state.currentOut = out_handler[x][y].state.min_range;
						continue;
					}
					if ( src_current == ( criteria | uint8_t(GOType_t::Gate) ) ){
						foundLane = true;
						out_handler[x][y].state.currentOut = out_handler[x][y].state.max_range;
						continue;
					} 
					if ( src_current == ( criteria | uint8_t(GOType_t::Velocity) ) ){
						foundLane = true;
						out_handler[x][y].state.currentOut = Rescale_16bit(msg->value, out_handler[x][y].state.min_range, out_handler[x][y].state.max_range);
						continue;
					}
				}
				if (foundLane){
					// Assume all controls are on single lane
					break;
				}
			}
		} else if(keyChannel == msg->channel) {
			uint8_t tempLane = 250;
			for (uint8_t x = 0; x < 4; x++){
				uint8_t lane = (currentKeyLane + x) & 0b11;
				if (keyLanes[lane].state == keyLanes_t::KeyPlaying && keyLanes[lane].note == msg->note){
					// Note is already playing
					return;
				}
				if (tempLane & 0x80){
					if (keyLanes[lane].state == keyLanes_t::KeyIdle){
						// Found an unused lane
						tempLane = lane;
						//break;
					} else if ((tempLane == 250) && (keyLanes[lane].state == keyLanes_t::KeyPlaying)){
						// Next lane in round-robin
						tempLane = lane | 0x80;
					}
				}
			}
				
			tempLane &= 0x7f;
				
			if (tempLane == 250){
				// No keys configured
				return;
			}
				
			// Update starting lane for Round-robin arbitration
			currentKeyLane = (tempLane + 1) & 0b11;
				
			if (keyLanes[tempLane].state == keyLanes_t::KeyPlaying){
				// Note already playing in lane. Push to queue
				noteQueue[queueIndex++].note = keyLanes[tempLane].note;
			}
				
			Start_Note(tempLane, msg->note, msg->value);
		}	
		return;
	} else if (msg->status == NOTE_OFF){
		// TODO: Trigger keylanes
		if (msg->channel == 9){
			// Drum channel
			uint32_t criteria = ( msg->channel << 8 ) | ( msg->note << 16 );
			bool foundLane = false;
			for (uint8_t x = 0; x < 4; x++){
				for (uint8_t y = 0; y < 4; y++){
					uint32_t src_current =
						( uint8_t(out_handler[x][y].state.type) << 0 ) |
						( out_handler[x][y].state.gen_source.channel << 8 ) |
						( out_handler[x][y].state.gen_source.sourceNum << 16 );
					if ( src_current == ( criteria | uint8_t(GOType_t::Envelope) ) ){
						foundLane = true;
						out_handler[x][y].state.envelope_stage = 4;
						continue;
					} 
					if ( src_current == ( criteria | uint8_t(GOType_t::Gate) ) ){
						foundLane = true;
						out_handler[x][y].state.currentOut = out_handler[x][y].state.min_range;
						continue;
					}
				}
				if (foundLane){
					// Assume all controls are on single lane
					break;
				}
			}
		} else if(keyChannel == msg->channel) {
			// Find the used lane
			uint8_t tempLane = 250;
			for (uint8_t x = 0; x < 4; x++){
				if (keyLanes[x].state == keyLanes_t::KeyPlaying){
					if (keyLanes[x].note == msg->note){
						tempLane = x;
						break;
					}
				}
			}
				
			// Look for note in queue
			uint8_t i;
			for (i = 0; i < queueIndex; i++){
				if (noteQueue[i].note == msg->note){
					queueIndex--;
					break;
				}
			}
			// Overwrite that note
			for (; i < queueIndex; i++){
				noteQueue[i] = noteQueue[i+1];
			}
				
			if (tempLane != 250){
				if (queueIndex > 0){
					// Start last note which was put in queue
					uint8_t tempNote = noteQueue[--queueIndex].note;
					Start_Note(tempLane, tempNote, msg->value);
				} else {
					// Stop note
					Stop_Note(tempLane);
				}
			}
		}	
		return;
	} else if (msg->status == PITCH_BEND){
		uint16_t tempBend = Rescale_16bit(msg->value >> 16, minBend, maxBend);
		currentBend = tempBend - 0x7fff;
				
		// update v/oct outputs
		uint32_t criteria = ( uint8_t(GOType_t::DC) << 0 ) | ( keyChannel << 8 ) | ( uint8_t(ctrlType_t::key) << 16 );
		for (uint8_t x = 0; x < 4; x++){
			if (keyLanes[x].state != keyLanes_t::KeyPlaying){
				continue;
			}
			for (uint8_t y = 0; y < 4; y++){
				uint32_t src_current = 
					( uint8_t(out_handler[x][y].state.type) << 0 ) | 
					( out_handler[x][y].state.gen_source.channel << 8 ) | 
					( uint8_t(out_handler[x][y].state.gen_source.sourceType) << 16 );
				if ( src_current == criteria ){
					out_handler[x][y].state.currentOut = Note_To_Output(keyLanes[x].note);
					break;
				}
			}
		}
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

