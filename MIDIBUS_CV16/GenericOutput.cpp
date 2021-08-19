/*
 * GenericOutput.cpp
 *
 * Created: 19/07/2021 18:09:51
 *  Author: GuavTek
 */ 

#include "GenericOutput.h"
#include "PWM.h"
#include "MIDI_Driver.h"

GenOut_t outMatrix[4][4];

bool keyVertical;
enum {
	KeyNone,
	KeyIdle,
	KeyPlaying
} keyLanes[4];
uint8_t currentKeyLane;
uint8_t keyChannel;

bool hasCC[4][4];

uint8_t group;

void GO_Init(){
	// Set default values
	PWM_Set(0,0, 0x3ff0);
	PWM_Set(0,1, 0x3ff0);
	PWM_Set(0,2, 0x3ff0);
	PWM_Set(0,3, 0x3ff0);
	PWM_Set(1,0, 0x3ff0);
	PWM_Set(1,1, 0x3ff0);
	PWM_Set(1,2, 0x3ff0);
	PWM_Set(1,3, 0x3ff0);
	PWM_Set(2,0, 0x3ff0);
	PWM_Set(2,1, 0x3ff0);
	PWM_Set(2,2, 0x3ff0);
	PWM_Set(2,3, 0x3ff0);
	PWM_Set(3,0, 0x3ff0);
	PWM_Set(3,1, 0x3ff0);
	PWM_Set(3,2, 0x3ff0);
	PWM_Set(3,3, 0x3ff0);
	
	outMatrix[2][2].type = GOType_t::LFO;
	outMatrix[2][2].shape = WavShape_t::Sawtooth;
	outMatrix[2][2].max_range = 0xffff;
	outMatrix[2][2].min_range = 0;
	outMatrix[2][2].direction = -1;
	outMatrix[2][2].freq_current = 0x0010 << 16;
	
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
	
	outMatrix[0][1].type = GOType_t::LFO;
	outMatrix[0][1].shape = WavShape_t::Square;
	outMatrix[0][1].max_range = 0xffff;
	outMatrix[0][1].min_range = 0;
	outMatrix[0][1].direction = -1;
	outMatrix[0][1].freq_current = 0x0040 << 16;
	
	outMatrix[0][2].type = GOType_t::LFO;
	outMatrix[0][2].shape = WavShape_t::Triangle;
	outMatrix[0][2].max_range = 0xffff;
	outMatrix[0][2].min_range = 0;
	outMatrix[0][2].direction = -1;
	outMatrix[0][2].freq_current = 0x0020 << 16;
	
	// Load setup from NVM
	
}

void GO_LFO(GenOut_t* go){
	go->freq_count += go->direction * go->freq_current;
	uint16_t tempOut = go->freq_count >> 16;
	if (go->shape == WavShape_t::Sawtooth){
		uint32_t remain = go->freq_count - (go->min_range << 16);
		if (remain < go->freq_current){
			uint32_t diff = go->freq_current - remain;
			go->freq_count = go->max_range - diff;
		}
		go->currentOut = go->freq_count >> 16;
	} else if (go->shape == WavShape_t::Square){
			uint32_t remain = 0xffffffff - go->freq_count;
			if (remain < go->freq_current){
				go->freq_count = go->freq_current - remain;
				if (go->currentOut == go->min_range){
					go->currentOut = go->max_range;
				} else {
					go->currentOut = go->min_range;
				}
			}
	} else {
		if (go->direction == 1){
			uint32_t remain = (go->max_range << 16) - go->freq_count;
			if (remain < go->freq_current){
				// change direction
				go->direction = -1;
				uint32_t diff = go->freq_current - remain;
				go->freq_count = go->max_range - diff;
			}
		} else {
			uint32_t remain = go->freq_count - (go->min_range << 16);
			if (remain < go->freq_current){
				go->direction = 1;
				uint32_t diff = go->freq_current - remain;
				go->freq_count = go->min_range + diff;
			}
		}
		
		if (go->shape == WavShape_t::Sine){
			go->currentOut = TriSine(go->freq_count >> 16);
		} else {
			go->currentOut = go->freq_count >> 16;
		}
	}
}

void GO_ENV(GenOut_t* go){
	switch(go->envelope_stage){
		uint16_t remain;
		case 1:
			// attack
			remain = go->max_range - go->currentOut;
			if (remain < go->att_current){
				go->envelope_stage++;
				go->currentOut = go->max_range;
			} else {
				go->currentOut += go->att_current;
			}
			break;
		case 2:
			// decay
			remain = go->currentOut - go->sus_current;
			if (remain < go->dec_current){
				go->envelope_stage++;
				go->currentOut = go->sus_current;
			} else {
				go->currentOut -= go->dec_current;
			}
			break;
		case 4:
			// release
			remain = go->currentOut - go->min_range;
			if (remain < go->rel_current){
				go->envelope_stage = 0;
				go->currentOut = go->min_range;
			} else {
				go->currentOut -= go->rel_current;
			}
			break;
		default:
			break;
void GO_Service(){
	}
}

uint16_t TriSine(uint16_t in){
	// Temporary
	return in;
}

