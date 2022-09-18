/*
 * GenericOutput.cpp
 *
 * Created: 19/07/2021 18:09:51
 *  Author: GuavTek
 */ 

#include "GenericOutput.h"
#include "LEDMatrix.h"
#include "PWM.h"
#include "MIDI_Driver.h"

bool needScan = false;

GenOut_t outMatrix[4][4];
Env_t envelopes[4];

struct keyLanes_t {
	uint8_t note;
	enum keyState_t {
		KeyNone = 0,
		KeyIdle = 1,
		KeyPlaying} state;
} keyLanes[4];
uint8_t currentKeyLane = 0;
uint8_t keyChannel = 1;
uint32_t keyPara;
uint8_t keyParaNum;

uint8_t bendRange;
int16_t currentBend;
uint16_t maxBend = 0x7fff + 3277;
uint16_t minBend = 0x7fff - 3277;

uint8_t queueIndex;
struct {
	uint8_t note;
	
} noteQueue[32];

// hasCC[4] is for envelopes
bool hasCC[5][4];

uint8_t midi_group = 1;

#define ENV_MANTISSA 7

#define INT_PER_VOLT 6553.6
#define INT_PER_NOTE INT_PER_VOLT/12
#define FIXED_POINT_POS 14
#define FIXED_INT_PER_NOTE ((uint32_t) INT_PER_NOTE * (1 << FIXED_POINT_POS))

// TODO: verify paraphonic configuration
// TODO: Setting gate on y > 1 breaks lanes. Sometimes???
// Scan the configuration
// To populate time saving variables
void Scan_Matrix(){
	// find Keychannel
	bool foundChannel = false;
	for(uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			if (outMatrix[x][y].gen_source.sourceType == ctrlType_t::Key){
				if (outMatrix[x][y].gen_source.channel != 9){
					keyChannel = outMatrix[x][y].gen_source.channel;
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
	uint8_t tempLanes = 0;
	int32_t lane_conf[4];
	for(uint8_t x = 0; x < 4; x++){
		lane_conf[x] = 0;
		for (uint8_t y = 0; y < 4; y++){
			// Get Keylane configuration
			if (outMatrix[x][y].gen_source.sourceType == ctrlType_t::Key){
				if (outMatrix[x][y].gen_source.channel == keyChannel){
					tempLanes |= 1 << x;
					lane_conf[x] += 1 << (4 * ((uint8_t) outMatrix[x][y].type));
				}
			}
			
			// Set LFO direction
			if (outMatrix[x][y].type == GOType_t::LFO){
				if (outMatrix[x][y].direction == 0){
					outMatrix[x][y].direction = 1;
				}
			}
		}
	}
	
	uint8_t min_diff = 69;
	uint32_t min_conf = 0;
	for (uint8_t i = 0; i < 4; i++){
		// Detect lane differences
		if (lane_conf[i]){
			for (uint8_t j = 0; j < i; j++){
				if (lane_conf[j]){
					uint8_t diff = 0;
					uint32_t diff_conf = 0;
					for (uint8_t k = 0; k < 6; k++){
						int8_t temp = (lane_conf[j] >> 4*k) & 0xf;
						temp -= (lane_conf[i] >> 4*k) & 0xf;
						if (temp < 0){
							diff -= temp;
						} else {
							diff += temp;
							diff_conf |= temp << 4*k;
						}
					}
					if (diff < min_diff){
						min_diff = diff;
						min_conf = lane_conf[j] - diff_conf;
					}
				}
			}
		}
	}
	
	// Find common outputs
	keyPara = 0;
	keyParaNum = 0;
	uint16_t matKey = (uint16_t) ctrlType_t::Key | (keyChannel << 8);
	for (uint8_t x = 0; x < 4; x++){
		// Has key outputs?
		if (lane_conf[x]){
			// Base config
			uint32_t tempConf = min_conf;
			uint8_t recNum = 0;
			uint16_t recPos = 0;
			for (uint8_t y = 0; y < 4; y++){
				uint16_t tempKey = (uint16_t) outMatrix[x][y].gen_source.sourceType | (outMatrix[x][y].gen_source.channel << 8);
				if (tempKey == matKey){
					int8_t temp = (tempConf >> (4 * ((uint8_t) outMatrix[x][y].type))) & 0xf;
					temp--;
					if (temp < 0){
						// Common output
						keyPara |= (y | (x << 2)) << (keyParaNum * 4);
						keyParaNum++;
					} else {
						// Maybe not common
						tempConf -= 1 << (4 * ((uint8_t) outMatrix[x][y].type));
						recPos |= (y | (x << 2)) << (recNum * 4);
						recNum++;
					}
				}
			}
			// tempConf != 0 -> is pure common lane
			if (tempConf){
				tempLanes &= ~(1 << x);
				// Recover outputs marked not common
				for (uint8_t i = 0; i < recNum; i++){
					keyPara |= ((recPos >> (4 * i)) & 0xf) << (4 * keyParaNum);
					keyParaNum++;
				}
			}
			// Update keylane states
			if (!((uint8_t) keyLanes[x].state) != !(tempLanes & (1 << x))){
				keyLanes[x].state = (keyLanes_t::keyState_t) ((tempLanes >> x) & 1);
			}
		}
	}
	
	// Find CC bound to outputs
	for (uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			if (outMatrix[x][y].gen_source.sourceType == ctrlType_t::None){
				hasCC[x][y] = 0;
			} else {
				hasCC[x][y] = 1;
			}
		}
	}
	
	// Detect CC in envelopes
	for (uint8_t i = 0; i < 4; i++){
		hasCC[4][i] = 0;
		if (envelopes[i].att_source.sourceType != ctrlType_t::None){
			hasCC[4][i] = 1;
			continue;
		}
		if (envelopes[i].dec_source.sourceType != ctrlType_t::None){
			hasCC[4][i] = 1;
			continue;
		}
		if (envelopes[i].sus_source.sourceType != ctrlType_t::None){
			hasCC[4][i] = 1;
			continue;
		}
		if (envelopes[i].rel_source.sourceType != ctrlType_t::None){
			hasCC[4][i] = 1;
			continue;
		}
	}
	
	// Configure note bend range
	maxBend = 0x7fff + 819 * bendRange;
	minBend = 0x7fff - 819 * bendRange;
	
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
	return mant << exp;
}

ufloat8_t uint32_to_ufloat8(uint32_t in){
	uint8_t exp = 29;
	uint8_t mant = 0;
	for (; exp > 0; exp--){
		mant = (in >> exp) & 0x07;
		if (mant >= 4){
			break;
		}
	}
	return (mant | (exp << 3));
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
	uint32_t tempResult = (maxOut-minOut+1) * val;
	tempResult >>= 16; // Divide by input range
	tempResult += minOut;
	return (uint16_t) tempResult;
}

inline void Start_Note(uint8_t lane, uint8_t note, uint16_t velocity){
	for (uint8_t y = 0; y < 4; y++){
		uint32_t src_current = ( uint8_t(outMatrix[lane][y].type) << 0 ) | ( uint8_t(outMatrix[lane][y].gen_source.sourceType) << 8 ) | ( outMatrix[lane][y].gen_source.channel << 16 );
		uint32_t criteria = ( uint8_t(GOType_t::DC) << 0 ) | ( uint8_t(ctrlType_t::Key) << 8 ) | ( keyChannel << 16 );
		if ( src_current == criteria ){
			outMatrix[lane][y].gen_source.sourceNum = note;
			outMatrix[lane][y].currentOut = Note_To_Output(note);
			continue;
		}
		
		criteria = ( uint8_t(GOType_t::Gate) << 0 ) | ( uint8_t(ctrlType_t::Key) << 8 ) | ( keyChannel << 16 );
		if ( src_current == criteria ){
			outMatrix[lane][y].gen_source.sourceNum = note;
			outMatrix[lane][y].currentOut = outMatrix[lane][y].max_range;
			continue;
		}
		
		criteria = ( uint8_t(GOType_t::Velocity) << 0 ) | ( uint8_t(ctrlType_t::Key) << 8 ) | ( keyChannel << 16 );
		if ( src_current == criteria ){
			outMatrix[lane][y].gen_source.sourceNum = note;
			outMatrix[lane][y].currentOut = Rescale_16bit(velocity, outMatrix[lane][y].min_range, outMatrix[lane][y].max_range);
			continue;
		}
		
		criteria = ( uint8_t(GOType_t::Envelope) << 0 ) | ( uint8_t(ctrlType_t::Key) << 8 ) | ( keyChannel << 16 );
		if ( src_current == criteria ){
			outMatrix[lane][y].gen_source.sourceNum = note;
			//outMatrix[lane][y].outCount = outMatrix[lane][y].min_range << 16;
			outMatrix[lane][y].envelope_stage = 1;
			continue;
		}
	}
	
	// Handle shared outputs
	for (uint8_t i = 0; i < keyParaNum; i++){
		uint8_t y = (keyPara >> (4*i));
		uint8_t x = (y >> 2) & 0b0011;
		y &= 0b0011;
		
		// Skip already handled outputs
		if (x == lane) continue;
		
		if ( outMatrix[x][y].type == GOType_t::DC ){
			outMatrix[x][y].gen_source.sourceNum = note;
			outMatrix[x][y].currentOut = Note_To_Output(note);
			continue;
		}
		
		if ( outMatrix[x][y].type == GOType_t::Gate ){
			outMatrix[x][y].gen_source.sourceNum = note;
			outMatrix[x][y].currentOut = outMatrix[x][y].max_range;
			continue;
		}
		
		if ( outMatrix[x][y].type == GOType_t::Velocity ){
			outMatrix[x][y].gen_source.sourceNum = note;
			outMatrix[x][y].currentOut = Rescale_16bit(velocity, outMatrix[x][y].min_range, outMatrix[x][y].max_range);
			continue;
		}
		
		if ( outMatrix[x][y].type == GOType_t::Envelope ){
			outMatrix[x][y].gen_source.sourceNum = note;
			//outMatrix[x][y].outCount = outMatrix[x][y].min_range << 16;
			outMatrix[x][y].envelope_stage = 1;
			continue;
		}
		
	}
	
	keyLanes[lane].state = keyLanes_t::KeyPlaying;
	keyLanes[lane].note = note;
}

inline void Stop_Note(uint8_t lane){
	for (uint8_t y = 0; y < 4; y++){
		uint32_t src_current = ( uint8_t(outMatrix[lane][y].type) << 0 ) | ( outMatrix[lane][y].gen_source.channel << 8 );
		if ( src_current == (uint8_t(GOType_t::Gate) | ( keyChannel << 8 )) ){
			outMatrix[lane][y].currentOut = outMatrix[lane][y].min_range;
			continue;
		} 
		
		if ( src_current == (uint8_t(GOType_t::Envelope) | (keyChannel << 8 )) ){
			outMatrix[lane][y].envelope_stage = 4;
			continue;
		}
	}
	
	// Handle shared outputs
	for (uint8_t i = 0; i < keyParaNum; i++){
		uint8_t y = (keyPara >> (4*i));
		uint8_t x = (y >> 2) & 0b0011;
		y &= 0b0011;
		
		// Skip already handled outputs
		if (x == lane) continue;
		
		if ( outMatrix[x][y].type == GOType_t::Gate ){
			outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
			continue;
		}
		
		if ( outMatrix[x][y].type == GOType_t::Envelope ){
			outMatrix[x][y].envelope_stage = 4;
			continue;
		}
		
	}
	
	keyLanes[lane].state = keyLanes_t::KeyIdle;
}

inline void Stop_All_Notes(){
	queueIndex = 0;
	for(uint8_t x = 0; x < 4; x++){
		if (keyLanes[x].state == keyLanes_t::KeyPlaying){
			keyLanes[x].state = keyLanes_t::KeyIdle;
		}
		for (uint8_t y = 0; y < 4; y++){
			if (outMatrix[x][y].type == GOType_t::Envelope){
				outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
				outMatrix[x][y].envelope_stage = 0;
			} else if (outMatrix[x][y].type == GOType_t::Gate){
				outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
			}
		}
	}
}

inline void Reset_All_Controllers(){
	for(uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			uint32_t src_current = ( uint8_t(outMatrix[x][y].type) << 0 ) | ( uint8_t(outMatrix[x][y].gen_source.sourceType) << 8 );
			uint32_t criteria = ( uint8_t(GOType_t::DC) << 0 ) | ( uint8_t(ctrlType_t::CC) << 8 );
			if ( src_current == criteria ){
				outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
			}
		}
	}
}

void GO_Init(){
	// Set default values
	outMatrix[0][0].currentOut = 0x7000;
	outMatrix[0][1].currentOut = 0x7000;
	outMatrix[0][2].currentOut = 0x7000;
	outMatrix[0][3].currentOut = 0x7000;
	outMatrix[1][0].currentOut = 0x7000;
	outMatrix[1][1].currentOut = 0x7000;
	outMatrix[1][2].currentOut = 0x7000;
	outMatrix[1][3].currentOut = 0x7000;
	outMatrix[2][0].currentOut = 0x7000;
	outMatrix[2][1].currentOut = 0x7000;
	outMatrix[2][2].currentOut = 0x7000;
	outMatrix[2][3].currentOut = 0x7000;
	outMatrix[3][0].currentOut = 0x7000;
	outMatrix[3][1].currentOut = 0x7000;
	outMatrix[3][2].currentOut = 0x7000;
	outMatrix[3][3].currentOut = 0x7000;
	
	// Temporary settings
	outMatrix[1][3].type = GOType_t::LFO;
	outMatrix[1][3].shape = WavShape_t::Sawtooth;
	outMatrix[1][3].max_range = 0x3fff;
	outMatrix[1][3].min_range = 0;
	outMatrix[1][3].direction = -1;
	outMatrix[1][3].freq_current = 0x0008 << 16;
	
	outMatrix[1][2].type = GOType_t::LFO;
	outMatrix[1][2].shape = WavShape_t::Sawtooth;
	outMatrix[1][2].max_range = 0xffff;
	outMatrix[1][2].min_range = 0x3fff;
	outMatrix[1][2].direction = -1;
	outMatrix[1][2].freq_current = 0x0010 << 16;
	outMatrix[1][2].freq_max = 0x01000000;
	outMatrix[1][2].freq_min = 0x00001000;
	outMatrix[1][2].gen_source.channel = 1;
	outMatrix[1][2].gen_source.sourceNum = 10;
	outMatrix[1][2].gen_source.sourceType = ctrlType_t::CC;
	hasCC[1][2] = 1;
	
	outMatrix[0][1].type = GOType_t::LFO;
	outMatrix[0][1].shape = WavShape_t::Square;
	outMatrix[0][1].max_range = 0xffff;
	outMatrix[0][1].min_range = 0;
	outMatrix[0][1].direction = 1;
	outMatrix[0][1].freq_current = 0x0040 << 16;
	
	outMatrix[0][2].type = GOType_t::LFO;
	outMatrix[0][2].shape = WavShape_t::Triangle;
	outMatrix[0][2].max_range = 0xffff;
	outMatrix[0][2].min_range = 0;
	outMatrix[0][2].direction = -1;
	outMatrix[0][2].freq_current = 0x0020 << 16;
	
	keyChannel = 1;
	keyLanes[0].state = keyLanes_t::KeyNone;
	keyLanes[1].state = keyLanes_t::KeyNone;
	keyLanes[2].state = keyLanes_t::KeyNone;
	keyLanes[3].state = keyLanes_t::KeyIdle;
	
	outMatrix[3][0].type = GOType_t::DC;
	outMatrix[3][0].gen_source.sourceType = ctrlType_t::Key;
	outMatrix[3][0].gen_source.channel = 1;
	outMatrix[3][0].max_range = 0xffff;
	outMatrix[3][0].min_range = 0;
	
	outMatrix[3][1].type = GOType_t::Gate;
	outMatrix[3][1].gen_source.sourceType = ctrlType_t::Key;
	outMatrix[3][1].gen_source.channel = 1;
	outMatrix[3][1].max_range = 0xffff;
	outMatrix[3][1].min_range = 0;
	
	outMatrix[3][2].type = GOType_t::Envelope;
	outMatrix[3][2].env_num = 0;
	outMatrix[3][2].max_range = 0xffff;
	outMatrix[3][2].min_range = 0;
	outMatrix[3][2].envelope_stage = 0;
	outMatrix[3][2].gen_source.sourceType = ctrlType_t::Key;
	outMatrix[3][2].gen_source.channel = 1;
	
	envelopes[0].att_current = 400;
	envelopes[0].att_max = 1;
	envelopes[0].att_min = 255;
	envelopes[0].att_source.sourceType = ctrlType_t::CC;
	envelopes[0].att_source.channel = 1;
	envelopes[0].att_source.sourceNum = 66;
	envelopes[0].dec_current = 600;
	envelopes[0].sus_current = 0xA000;
	envelopes[0].rel_current = 0xffff;
	hasCC[4][0] = 1;
	
	// Load setup from NVM
	
	
	// Fill out utility variables
	Scan_Matrix();
	
}

void GO_LFO(GenOut_t* go){
	go->outCount += go->direction * go->freq_current;
	uint16_t tempOut = go->outCount >> 16;
	if (go->shape == WavShape_t::Sawtooth){
		uint32_t remain = go->outCount - (go->min_range << 16);
		if (remain < go->freq_current){
			uint32_t diff = go->freq_current - remain;
			go->outCount = go->max_range - diff;
		}
		go->currentOut = go->outCount >> 16;
	} else if (go->shape == WavShape_t::Square){
			uint32_t remain = 0xffffffff - go->outCount;
			if (remain < go->freq_current){
				go->outCount = go->freq_current - remain;
				if (go->currentOut == go->min_range){
					go->currentOut = go->max_range;
				} else {
					go->currentOut = go->min_range;
				}
			}
	} else {
		if (go->direction == 1){
			uint32_t remain = (go->max_range << 16) - go->outCount;
			if (remain < go->freq_current){
				// change direction
				go->direction = -1;
				uint32_t diff = go->freq_current - remain;
				go->outCount = go->max_range - diff;
			}
		} else {
			uint32_t remain = go->outCount - (go->min_range << 16);
			if (remain < go->freq_current){
				go->direction = 1;
				uint32_t diff = go->freq_current - remain;
				go->outCount = go->min_range + diff;
			}
		}
		
		if (go->shape == WavShape_t::Sine){
			go->currentOut = TriSine(go->outCount >> 16);
		} else {
			go->currentOut = go->outCount >> 16;
		}
	}
}

// TODO: Envelopes only work reliably on full-scale
void GO_ENV(GenOut_t* go){
	Env_t* tempEnv = &envelopes[go->env_num];
	switch(go->envelope_stage){
		uint32_t remain;
		case 1:
			// attack
			remain = (go->max_range - go->currentOut) << ENV_MANTISSA;
			if (remain < tempEnv->att_current){
				go->envelope_stage++;
				go->outCount = go->max_range << 16;
			} else {
				go->outCount += tempEnv->att_current << (16 - ENV_MANTISSA);
			}
			break;
		case 2:
			// decay
			remain = (go->currentOut - tempEnv->sus_current) << ENV_MANTISSA;
			if (remain < tempEnv->dec_current){
				go->envelope_stage++;
				go->outCount = tempEnv->sus_current << 16;
			} else {
				go->outCount -= tempEnv->dec_current << (16 - ENV_MANTISSA);
			}
			break;
		case 4:
			// release
			remain = (go->currentOut - go->min_range) << ENV_MANTISSA;
			if (remain < tempEnv->rel_current){
				go->envelope_stage = 0;
				go->outCount = go->min_range << 16;
			} else {
				go->outCount -= tempEnv->rel_current << (16 - ENV_MANTISSA);
			}
			break;
		default:
			break;
	}
	// Set new value
	go->currentOut = go->outCount >> 16;
}

void GO_MIDI_Voice(MIDI2_voice_t* msg){
	if(msg->group != midi_group){
		return;
	}
	
	enum ctrlType_t msgType;
	uint16_t controlNum = 0;
	switch(msg->status){
		case MIDI2_VOICE_E::AssControl:
			controlNum = msg->bankCtrl << 8;
		case MIDI2_VOICE_E::CControl:
			controlNum |= msg->index;
			msgType = ctrlType_t::CC;
			break;
		case MIDI2_VOICE_E::Aftertouch:
		case MIDI2_VOICE_E::ChanPressure:
		case MIDI2_VOICE_E::NoteOff:
		case MIDI2_VOICE_E::NoteOn:
		case MIDI2_VOICE_E::Pitchbend:
			msgType = ctrlType_t::Key;
			break;
		case MIDI2_VOICE_E::ProgChange:
			msgType = ctrlType_t::PC;
			break;
		default:
			return;
	};
	
	if (msgType == ctrlType_t::CC){
		if (msg->status == MIDI2_VOICE_E::CControl){
			if ((msg->controller == 120)||(msg->controller == 123)){
				Stop_All_Notes();
				return;
			} else if (msg->controller == 121){
				Reset_All_Controllers();
				return;
			}
		}
		
		// Search outputs
		for (uint8_t x = 0; x < 4; x++){
			for (uint8_t y = 0; y < 4; y++){
				// Skip if no CC is mapped
				if (!hasCC[x][y]){
					continue;
				}
				
				uint32_t src_current = ( uint8_t(outMatrix[x][y].type) << 0 ) | ( outMatrix[x][y].gen_source.channel << 8 ) | ( outMatrix[x][y].gen_source.sourceNum << 16 );
				uint32_t criteria = ( uint8_t(GOType_t::DC) << 0 ) | ( msg->channel << 8 ) | ( controlNum << 16 );
				if ( src_current == criteria ){
					// CV generation
					uint32_t span = (outMatrix[x][y].max_range - outMatrix[x][y].min_range) + 1;
					uint32_t scaled = (msg->data >> 16) * span;
					outMatrix[x][y].currentOut = (scaled >> 16) + outMatrix[x][y].min_range;
					continue;
				}
				
				criteria = ( uint8_t(GOType_t::LFO) << 0 ) | ( msg->channel << 8 ) | ( controlNum << 16 );
				if ( src_current == criteria ){
					// Wave generation
					uint64_t span = outMatrix[x][y].freq_max - outMatrix[x][y].freq_min + 1;
					uint64_t scaled = msg->data * span;
					outMatrix[x][y].freq_current = (scaled >> 32) + outMatrix[x][y].freq_min;
					continue;					
				}	
			}
		}
		
		// Search envelopes
		uint32_t criteria = ( uint8_t(ctrlType_t::CC) << 0 ) | ( msg->channel << 8 ) | ( controlNum << 16 );
		for (uint8_t i = 0; i < 4; i++){
			if (!hasCC[4][i]){
				continue;
			}
			
			// Attack
			uint32_t src_current = 
				( uint8_t(envelopes[i].att_source.sourceType) << 0 ) | 
				( envelopes[i].att_source.channel << 8 ) | 
				( envelopes[i].att_source.sourceNum << 16 );
			if (src_current == criteria){
				if (envelopes[i].att_max > envelopes[i].att_min){
					uint32_t diff = envelopes[i].att_max - envelopes[i].att_min;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->data >> 16) * span;
					envelopes[i].att_current = 0x0101 * envelopes[i].att_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].att_min - envelopes[i].att_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->data >> 16) * span;
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
					uint32_t scaled = (msg->data >> 16) * span;
					envelopes[i].dec_current = 0x0101 * envelopes[i].dec_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].dec_min - envelopes[i].dec_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->data >> 16) * span;
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
					uint32_t scaled = (msg->data >> 16) * span;
					envelopes[i].sus_current = 0x0101 * envelopes[i].sus_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].sus_min - envelopes[i].sus_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->data >> 16) * span;
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
					uint32_t scaled = (msg->data >> 16) * span;
					envelopes[i].rel_current = 0x0101 * envelopes[i].rel_min + (scaled >> 16);
				} else {
					uint32_t diff = envelopes[i].rel_min - envelopes[i].rel_max;
					uint32_t span = diff * 0x0101 + 1;
					uint32_t scaled = (msg->data >> 16) * span;
					envelopes[i].rel_current = 0x0101 * envelopes[i].rel_min - (scaled >> 16);
				}				
			}
		}
	} else if(msgType == ctrlType_t::PC){
		uint32_t criteria = (uint8_t(GOType_t::DC)) | ( msg->channel << 8 ) | ( msg->program << 16 );
		for (uint8_t x = 0; x < 4; x++){
			for (uint8_t y = 0; y < 4; y++){
				uint32_t src_current =
				( uint8_t(outMatrix[x][y].type) << 0 ) |
				( outMatrix[x][y].gen_source.channel << 8 ) |
				( outMatrix[x][y].gen_source.sourceNum << 16 );
				if ( src_current == criteria ){
					if (outMatrix[x][y].currentOut == outMatrix[x][y].max_range){
						outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
					} else {
						outMatrix[x][y].currentOut = outMatrix[x][y].max_range;
					}
					return;
				}
			}
		}
	} else {
		if (msg->channel == 9){
			// Drum channel
			uint32_t criteria = ( msg->channel << 8 ) | ( msg->note << 16 );
			if (msg->status == MIDI2_VOICE_E::NoteOn){
				bool foundLane = false;
				for (uint8_t x = 0; x < 4; x++){
					for (uint8_t y = 0; y < 4; y++){
						uint32_t src_current = 
							( uint8_t(outMatrix[x][y].type) << 0 ) |
							( outMatrix[x][y].gen_source.channel << 8 ) |
							( outMatrix[x][y].gen_source.sourceNum << 16 );
						if ( src_current == ( criteria | uint8_t(GOType_t::Envelope) ) ){
							foundLane = true;
							outMatrix[x][y].envelope_stage = 1;
							outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
							continue;
						}
						if ( src_current == ( criteria | uint8_t(GOType_t::Gate) ) ){
							foundLane = true;
							outMatrix[x][y].currentOut = outMatrix[x][y].max_range;
							continue;
						} 
						if ( src_current == ( criteria | uint8_t(GOType_t::Velocity) ) ){
							foundLane = true;
							outMatrix[x][y].currentOut = Rescale_16bit(msg->velocity, outMatrix[x][y].min_range, outMatrix[x][y].max_range);
							continue;
						}
					}
					if (foundLane){
						// Assume all controls are on single lane
						break;
					}
				}
			} else if (msg->status == MIDI2_VOICE_E::NoteOff){
				bool foundLane = false;
				for (uint8_t x = 0; x < 4; x++){
					for (uint8_t y = 0; y < 4; y++){
						uint32_t src_current =
							( uint8_t(outMatrix[x][y].type) << 0 ) |
							( outMatrix[x][y].gen_source.channel << 8 ) |
							( outMatrix[x][y].gen_source.sourceNum << 16 );
						if ( src_current == ( criteria | uint8_t(GOType_t::Envelope) ) ){
							foundLane = true;
							outMatrix[x][y].envelope_stage = 4;
							continue;
						} 
						if ( src_current == ( criteria | uint8_t(GOType_t::Gate) ) ){
							foundLane = true;
							outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
							continue;
						}
					}
					if (foundLane){
						// Assume all controls are on single lane
						break;
					}
				}
			}
		} else if(keyChannel == msg->channel) {
			if (msg->status == MIDI2_VOICE_E::NoteOn){
				uint8_t tempLane = 250;
				// TODO: Check if note is already playing
				for (uint8_t x = 0; x < 4; x++){
					uint8_t lane = (currentKeyLane + x) & 0b11;
					if (keyLanes[lane].state == keyLanes_t::KeyIdle){
						// Found an unused lane
						tempLane = lane;
						break;
					} else if ((tempLane == 250)&&(keyLanes[lane].state == keyLanes_t::KeyPlaying)){
						tempLane = lane;
					}
				}
				
				if (tempLane == 250){
					// No keys
					return;
				}
				
				currentKeyLane = (tempLane + 1) & 0b11;
				
				if (keyLanes[tempLane].state == keyLanes_t::KeyPlaying){
					// Note already playing. Push to queue
					noteQueue[queueIndex++].note = keyLanes[tempLane].note;
				}
				
				Start_Note(tempLane, msg->note, msg->velocity);
				
			} else if (msg->status == MIDI2_VOICE_E::NoteOff){
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
						Start_Note(tempLane, tempNote, msg->velocity);
					} else {
						// Stop note
						Stop_Note(tempLane);
					}
				}
			} else if (msg->status == MIDI2_VOICE_E::Pitchbend){
				uint16_t  tempBend = Rescale_16bit(msg->data >> 16, minBend, maxBend);
				currentBend = tempBend - 0x7fff;
				
				// update v/oct outputs
				uint32_t criteria = ( uint8_t(GOType_t::DC) << 0 ) | ( keyChannel << 8 ) | ( uint8_t(ctrlType_t::Key) << 16 );
				for (uint8_t x = 0; x < 4; x++){
					if (keyLanes[x].state != keyLanes_t::KeyPlaying){
						continue;
					}
					for (uint8_t y = 0; y < 4; y++){
						uint32_t src_current = 
							( uint8_t(outMatrix[x][y].type) << 0 ) | 
							( outMatrix[x][y].gen_source.channel << 8 ) | 
							( uint8_t(outMatrix[x][y].gen_source.sourceType) << 16 );
						if ( src_current == criteria ){
							outMatrix[x][y].currentOut = Note_To_Output(keyLanes[x].note);
							break;
						}
					}
				}
			} else if (msg->status == MIDI2_VOICE_E::ChanPressure){
				uint32_t criteria = ( uint8_t(GOType_t::Pressure) << 0 ) | ( keyChannel << 8 ) | ( uint8_t(ctrlType_t::Key) << 16 );
				for (uint8_t x = 0; x < 4; x++){
					for (uint8_t y = 0; y < 4; y++){
						uint32_t src_current = 
							( uint8_t(outMatrix[x][y].type) << 0 ) | 
							( outMatrix[x][y].gen_source.channel << 8 ) | 
							( uint8_t(outMatrix[x][y].gen_source.sourceType) << 16 );
						if ( src_current == criteria ){
							outMatrix[x][y].currentOut = Rescale_16bit(msg->data >> 16, outMatrix[x][y].min_range, outMatrix[x][y].max_range);
							break;
						}
					}
				}
			} else if (msg->status == MIDI2_VOICE_E::Aftertouch){
				uint32_t criteria = ( uint8_t(GOType_t::Pressure) << 0 ) | ( keyChannel << 8 ) | ( uint8_t(ctrlType_t::Key) << 16 ) | ( msg->note << 24 );
				for (uint8_t x = 0; x < 4; x++){
					for (uint8_t y = 0; y < 4; y++){
						uint32_t src_current = 
							( uint8_t(outMatrix[x][y].type) << 0 ) | 
							( outMatrix[x][y].gen_source.channel << 8 ) | 
							( uint8_t(outMatrix[x][y].gen_source.sourceType) << 16 ) |
							( keyLanes[x].note << 24 );
						if ( src_current == criteria ){
							outMatrix[x][y].currentOut = Rescale_16bit(msg->data >> 16, outMatrix[x][y].min_range, outMatrix[x][y].max_range);
							break;
						}
					}
				}
			}
		}		
	}
	return;
}

void GO_MIDI_Realtime(MIDI2_com_t* msg){
	if(msg->status == MIDI2_COM_E::TimingClock){
		for (uint8_t x = 0; x < 4; x++){
			for (uint8_t y = 0; y < 4; y++){
				if (outMatrix[x][y].type == GOType_t::CLK){
					outMatrix[x][y].outCount++;
					if (outMatrix[x][y].outCount > outMatrix[x][y].freq_current){
						outMatrix[x][y].outCount = 0;
						outMatrix[x][y].currentOut = (outMatrix[x][y].currentOut == outMatrix[x][y].max_range) ? outMatrix[x][y].min_range : outMatrix[x][y].max_range;
					}
				}
			}
		}
	}
}

void GO_Service(){
	// Update configuration when needed
	if (needScan) Scan_Matrix();
	
	for (uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			switch(outMatrix[x][y].type){
				case GOType_t::LFO:
					GO_LFO(&outMatrix[x][y]);
					break;
				case GOType_t::Envelope:
					GO_ENV(&outMatrix[x][y]);
					break;
				default:
					break;
			}
			PWM_Set(x,y,outMatrix[x][y].currentOut);
		}
	}
}

void GO_Service(uint8_t x){
	// Update configuration when needed
	if (needScan) Scan_Matrix();
	
	for (uint8_t y = 0; y < 4; y++){
		switch(outMatrix[x][y].type){
		case GOType_t::LFO:
			GO_LFO(&outMatrix[x][y]);
			break;
		case GOType_t::Envelope:
			GO_ENV(&outMatrix[x][y]);
			break;
		default:
			break;
		}
		PWM_Set(x,y,outMatrix[x][y].currentOut);
	}
}

// TODO: Fix glitch in wave
// Return sine output from linear input
uint16_t TriSine(uint16_t in){
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
	
	return tempOut;
}

