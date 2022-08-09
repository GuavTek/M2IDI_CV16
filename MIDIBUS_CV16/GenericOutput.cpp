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

GenOut_t outMatrix[4][4];
Env_t envelopes[4];

struct keyLanes_t {
	uint8_t note;
	enum {
		KeyNone,
		KeyIdle,
		KeyPlaying} state;
} keyLanes[4];
uint8_t currentKeyLane = 0;
uint8_t keyChannel = 1;

int16_t currentBend;
uint16_t maxBend = 0x7fff + 3277;
uint16_t minBend = 0x7fff - 3277;

uint8_t queueIndex;
struct {
	uint8_t note;
	
} noteQueue[32];

// hasCC[4] is for envelopes
bool hasCC[5][4];

uint8_t group = 1;

#define ENV_MANTISSA 7

#define INT_PER_VOLT 6553.6
#define INT_PER_NOTE INT_PER_VOLT/12
#define FIXED_POINT_POS 14
#define FIXED_INT_PER_NOTE ((uint32_t) INT_PER_NOTE * (1 << FIXED_POINT_POS))

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
		if (outMatrix[lane][y].type == GOType_t::DC){
			if (outMatrix[lane][y].dc_source.sourceType == ctrlType_t::Key){
				if (keyChannel == outMatrix[lane][y].dc_source.channel){
					outMatrix[lane][y].dc_source.sourceNum = note;
					outMatrix[lane][y].currentOut = Note_To_Output(note);
				}
			}
		} else if (outMatrix[lane][y].type == GOType_t::Gate){
			if (keyChannel == outMatrix[lane][y].dc_source.channel){
				outMatrix[lane][y].dc_source.sourceNum = note;
				outMatrix[lane][y].currentOut = outMatrix[lane][y].max_range;
			}
		} else if (outMatrix[lane][y].type == GOType_t::Velocity){
			if (keyChannel == outMatrix[lane][y].dc_source.channel){
				outMatrix[lane][y].currentOut = Rescale_16bit(velocity, outMatrix[lane][y].min_range, outMatrix[lane][y].max_range);
			}
		} else if (outMatrix[lane][y].type == GOType_t::Envelope){
			if (keyChannel == outMatrix[lane][y].env_source.channel){
				outMatrix[lane][y].envelope_stage = 1;
			}
			//outMatrix[lane][y].outCount = outMatrix[lane][y].min_range << 16;
		}
	}
	
	keyLanes[lane].state = keyLanes_t::KeyPlaying;
	keyLanes[lane].note = note;
}

inline void Stop_Note(uint8_t lane){
	for (uint8_t y = 0; y < 4; y++){
		if (outMatrix[lane][y].type == GOType_t::Gate){
			if (keyChannel == outMatrix[lane][y].dc_source.channel){
				outMatrix[lane][y].currentOut = outMatrix[lane][y].min_range;
			}
		} else if (outMatrix[lane][y].type == GOType_t::Envelope){
			if (keyChannel == outMatrix[lane][y].env_source.channel){
				outMatrix[lane][y].envelope_stage = 4;
			}
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
			if (outMatrix[x][y].type == GOType_t::DC){
				if (outMatrix[x][y].dc_source.sourceType == ctrlType_t::CC){
					outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
				}
				
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
	outMatrix[1][2].freq_source.channel = 1;
	outMatrix[1][2].freq_source.sourceNum = 10;
	outMatrix[1][2].freq_source.sourceType = ctrlType_t::CC;
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
	outMatrix[3][0].dc_source.sourceType = ctrlType_t::Key;
	outMatrix[3][0].dc_source.channel = 1;
	outMatrix[3][0].max_range = 0xffff;
	outMatrix[3][0].min_range = 0;
	
	outMatrix[3][1].type = GOType_t::Gate;
	outMatrix[3][1].dc_source.sourceType = ctrlType_t::Key;
	outMatrix[3][1].dc_source.channel = 1;
	outMatrix[3][1].max_range = 0xffff;
	outMatrix[3][1].min_range = 0;
	
	outMatrix[3][2].type = GOType_t::Envelope;
	outMatrix[3][2].env_num = 0;
	outMatrix[3][2].max_range = 0xffff;
	outMatrix[3][2].min_range = 0;
	outMatrix[3][2].envelope_stage = 0;
	outMatrix[3][2].env_source.sourceType = ctrlType_t::Key;
	outMatrix[3][2].env_source.channel = 1;
	
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
	for (uint8_t x = 0; x < 4; x++){
		for (uint8_t y = 0; y < 4; y++){
			
		}
	}
	
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
	if(msg->group != group){
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
		default:
			return;
	};
	
	// Search outputs
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
		
		
		for (uint8_t x = 0; x < 4; x++){
			for (uint8_t y = 0; y < 4; y++){
				// Skip if no CC is mapped
				if (!hasCC[x][y]){
					continue;
				}
				
				if (outMatrix[x][y].type == GOType_t::DC){
					// CV generation
					if (outMatrix[x][y].dc_source.channel == msg->channel){
						if (outMatrix[x][y].dc_source.sourceNum == controlNum){
							uint32_t span = (outMatrix[x][y].max_range - outMatrix[x][y].min_range) + 1;
							uint32_t scaled = (msg->data >> 16) * span;
							outMatrix[x][y].currentOut = (scaled >> 16) + outMatrix[x][y].min_range;
						}
					}
				} else if (outMatrix[x][y].type == GOType_t::LFO){
					// Wave generation
					if (outMatrix[x][y].freq_source.channel == msg->channel){
						if (outMatrix[x][y].freq_source.sourceNum == controlNum){
							uint64_t span = outMatrix[x][y].freq_max - outMatrix[x][y].freq_min + 1;
							uint64_t scaled = msg->data * span;
							outMatrix[x][y].freq_current = (scaled >> 32) + outMatrix[x][y].freq_min;
						}
					}
				}	
			}
		}
		
		for (uint8_t i = 0; i < 4; i++){
			if (!hasCC[4][i]){
				continue;
			}
			
			if (envelopes[i].att_source.sourceType == ctrlType_t::CC){
				if (envelopes[i].att_source.channel == msg->channel){
					if (envelopes[i].att_source.sourceNum == controlNum){
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
				}
			}
			if (envelopes[i].dec_source.sourceType == ctrlType_t::CC){
				if (envelopes[i].dec_source.channel == msg->channel){
					if (envelopes[i].dec_source.sourceNum == controlNum){
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
				}
			}
			if (envelopes[i].sus_source.sourceType == ctrlType_t::CC){
				if (envelopes[i].sus_source.channel == msg->channel){
					if (envelopes[i].sus_source.sourceNum == controlNum){
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
				}
			}
			if (envelopes[i].rel_source.sourceType == ctrlType_t::CC){
				if (envelopes[i].rel_source.channel == msg->channel){
					if (envelopes[i].rel_source.sourceNum == controlNum){
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
			}
		}
	} else {
		if (msg->channel == 9){
			// Drum channel
			if (msg->status == MIDI2_VOICE_E::NoteOn){
				bool foundLane = false;
				for (uint8_t x = 0; x < 4; x++){
					for (uint8_t y = 0; y < 4; y++){
						if (outMatrix[x][y].type == GOType_t::Envelope){
							if (outMatrix[x][y].env_source.channel == msg->channel){
								if (outMatrix[x][y].env_source.sourceNum == msg->note){
									foundLane = true;
									outMatrix[x][y].envelope_stage = 1;
									outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
								}
							}
						} else {
							if (outMatrix[x][y].dc_source.channel == msg->channel){
								if (outMatrix[x][y].dc_source.sourceNum == msg->note){
									if (outMatrix[x][y].type == GOType_t::Gate){
										foundLane = true;
										outMatrix[x][y].currentOut = outMatrix[x][y].max_range;
									} else if (outMatrix[x][y].type == GOType_t::Velocity){
										foundLane = true;
										outMatrix[x][y].currentOut = Rescale_16bit(msg->velocity, outMatrix[x][y].min_range, outMatrix[x][y].max_range);
									}
								}
							}
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
						if (outMatrix[x][y].type == GOType_t::Envelope){
							if (outMatrix[x][y].env_source.channel == msg->channel){
								if (outMatrix[x][y].env_source.sourceNum == msg->note){
									foundLane = true;
									outMatrix[x][y].envelope_stage = 4;
								}
							}
						} else if (outMatrix[x][y].type == GOType_t::Gate) {
							if (outMatrix[x][y].dc_source.channel == msg->channel){
								if (outMatrix[x][y].dc_source.sourceNum == msg->note){
									foundLane = true;
									outMatrix[x][y].currentOut = outMatrix[x][y].min_range;
								}
							}
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
				for (uint8_t x = 0; x < 4; x++){
					uint8_t lane = (currentKeyLane + x) & 0b11;
					if (keyLanes[lane].state == keyLanes_t::KeyIdle){
						// Found a unused lane
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
				if (tempLane == 250){
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
				} else {
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
				
				
				
				// update outputs
				for (uint8_t x = 0; x < 4; x++){
					if (keyLanes[x].state != keyLanes_t::KeyPlaying){
						continue;
					}
					for (uint8_t y = 0; y < 4; y++){
						if (outMatrix[x][y].type == GOType_t::DC){
							if (outMatrix[x][y].dc_source.channel == keyChannel){
								if (outMatrix[x][y].dc_source.sourceType == ctrlType_t::Key){
									outMatrix[x][y].currentOut = Note_To_Output(keyLanes[x].note);
									break;
								}
							}
						}
					}
				}
			} else if (msg->status == MIDI2_VOICE_E::ChanPressure){
				for (uint8_t x = 0; x < 4; x++){
					for (uint8_t y = 0; y < 4; y++){
						if (outMatrix[x][y].type == GOType_t::Pressure){
							outMatrix[x][y].currentOut = Rescale_16bit(msg->data >> 16, outMatrix[x][y].min_range, outMatrix[x][y].max_range);
							return;
						}
					}
				}
			} else if (msg->status == MIDI2_VOICE_E::Aftertouch){
				
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

