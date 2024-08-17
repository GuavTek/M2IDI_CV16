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
#include "rp_rand.h"

bool needScan = false;

env_handler_c envelopes[4];

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
//PrintConst<sizeof(ConfigNVM_t)> p;

uint8_t midi_group = 1;

#define ENV_MANTISSA 7

#define OUTPUT_GAIN 1		// Nominal gain
#define INT_PER_VOLT 6553.6/OUTPUT_GAIN
#define INT_PER_NOTE INT_PER_VOLT/12
#define FIXED_POINT_POS 16
#define FIXED_VOLT_PER_INT ((uint32_t) ((1/INT_PER_VOLT) * (1 << FIXED_POINT_POS)))
#define FIXED_INT_PER_NOTE ((uint32_t) (INT_PER_NOTE * (1 << FIXED_POINT_POS)))

// Python cubic curve fit: 0.05707194*x^3 + 0.2489873*x^2 + 0.69279463*x + 0.99907843
// Python square fit: 0.2489873*x^2 + 0.72872142*x + 0.99907843
// Fixed point exp2 (Only valid for range [-16,15]
uint32_t fp_exp2(int32_t val){
	int32_t ret_val;
	int32_t fmant;
	int64_t temp64;
	int32_t exp_mant;
	int32_t fint;
	fint = val >> FIXED_POINT_POS;
	if (val >= 0) {
		fmant = val & ~(0xffffffff << FIXED_POINT_POS);
	} else {
		fmant = val | (0xffffffff << FIXED_POINT_POS);
		fint += 1;
	}
	exp_mant = fmant;
	// Mantissa part of the value
	ret_val = int32_t(0.99907843 * (1 << FIXED_POINT_POS));
	// 1st coeff
	int32_t temp_val;
	temp64 = int32_t(0.69279463 * (1 << FIXED_POINT_POS));
	temp64 *= exp_mant;
	ret_val += temp64 >> FIXED_POINT_POS;
	// 2nd coeff
	temp_val = int32_t(0.2489873 * (1 << FIXED_POINT_POS));
	temp64 = exp_mant;
	temp64 *= fmant;
	exp_mant = temp64 >> FIXED_POINT_POS;
	temp_val *= exp_mant;
	temp_val >>= FIXED_POINT_POS;
	ret_val += temp_val;
	// 3rd coeff
	temp_val = int32_t(0.05707194 * (1 << FIXED_POINT_POS));
	temp64 = exp_mant;
	temp64 *= fmant;
	exp_mant = temp64 >> FIXED_POINT_POS;
	temp_val *= exp_mant;
	temp_val >>= FIXED_POINT_POS;
	ret_val += temp_val;
	// Integer part of fixed point value
	temp64 = 1 << (fint + FIXED_POINT_POS);
	temp64 *= ret_val;
	temp64 >>= FIXED_POINT_POS;
	return uint32_t(temp64);
}

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

	for (uint8_t i = 0; i < 8; i++){
		if (lane_map[i+1] > 0){
			key_handler.lane_map[i] = lane_map[i+1]-1;
		} else {
			key_handler.lane_map[i] = 0;
		}
	}

	needScan = false;
}

// Low precision floating point to save memory space
#define ufloat8_t uint8_t
uint32_t ufloat8_to_uint32(ufloat8_t in){
	int8_t exp = in >> 3;
	uint8_t mant = in & 0x07;
	if (exp > 29){
		return mant;
	}
	uint32_t out_val = 0x08 << exp;
	// Extend value increase dynamic range
	for (; exp > 0; exp -= 3){
		out_val |= mant << exp;
	}
	out_val |= mant >> -exp;
	return out_val;
}

ufloat8_t uint32_to_ufloat8(uint32_t in){
	uint8_t exp = 29;
	uint8_t mant = 0;
	if (in < 0x8) {
		exp = 0x1f;
		mant = in;
	} else {
		// Find MSb
		for (exp = 31; exp > 3; exp--){
			if (in & (1 << exp)){
				break;
			}
		}
		exp -= 3;
		mant = (in >> exp) & 0x0f;
	}
	return ((mant & 0x07) | (exp << 3));
}

inline uint16_t Note_To_Output(uint8_t note){
	// C4 is middle note -> 60 = 0V
	int32_t tempOut;
	if (note > 119) {
		tempOut = 0xffff;
	} else {
		tempOut = (note-60) * FIXED_INT_PER_NOTE;// * 546.133 fixed point multiplication
		tempOut += 1 << (FIXED_POINT_POS - 1);	// + 0.5 to round intead of floor
		tempOut >>= FIXED_POINT_POS;	// Round to int
		tempOut += 0x7fff;
	}

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
	// Fill out utility variables
	Scan_Matrix();

}

void GO_Default_Config(){
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
	out_handler[0][3].state.freq_max = 0x0010 << 16;
	out_handler[0][3].state.freq_min = 0x0002 << 16;
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

	out_handler[0][1].set_type(GOType_t::LFO);
	out_handler[0][1].state.shape = WavShape_t::Square;
	out_handler[0][1].state.max_range = 0xffff;
	out_handler[0][1].state.min_range = 0;
	out_handler[0][1].state.direction = 1;
	out_handler[0][1].state.freq_current = FREQS.f1Hz; // 0x0040 << 16;
	out_handler[0][1].state.freq_max = FREQS.f1Hz;
	out_handler[0][1].state.freq_min = FREQS.f1Hz;
	out_handler[0][2].set_type(GOType_t::LFO);
	out_handler[0][2].state.shape = WavShape_t::Triangle;
	out_handler[0][2].state.max_range = 0xffff;
	out_handler[0][2].state.min_range = 0;
	out_handler[0][2].state.direction = -1;
	out_handler[0][2].state.freq_current = FREQS.midi[69]; // 0x0020 << 16;
	out_handler[0][2].state.freq_max = 0x0010 << 16;
	out_handler[0][2].state.freq_min = 0x0002 << 16;

	key_handler.set_key_channel(1);
	key_handler.set_bend_range(11);

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
	out_handler[3][2].state.envelope_stage = EnvStage_t::idle;
	out_handler[3][2].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[3][2].state.gen_source.channel = 0;
	out_handler[3][3].set_type(GOType_t::CLK);
	out_handler[3][3].state.max_range = 0xffff;
	out_handler[3][3].state.min_range = 0;
	out_handler[3][3].state.freq_current = 23;
	out_handler[3][3].state.freq_min = 23;
	out_handler[3][3].state.freq_max = 23;
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
	out_handler[2][2].state.envelope_stage = EnvStage_t::idle;
	out_handler[2][2].state.gen_source.sourceType = ctrlType_t::key;
	out_handler[2][2].state.gen_source.channel = 0;
	out_handler[1][3].set_type(GOType_t::DC);
	out_handler[1][3].state.gen_source.sourceType = ctrlType_t::controller;
	out_handler[1][3].state.gen_source.channel = 0;
	out_handler[1][3].state.gen_source.sourceNum = 20;
	out_handler[1][3].state.max_range = 0xffff;
	out_handler[1][3].state.currentOut = 0xffff;
	out_handler[1][3].state.min_range = 0;

	envelopes[0].env.att.current = 0x3000'0000;
	envelopes[0].env.att.max = 0x3000'0000;
	envelopes[0].env.att.min = 255;
	envelopes[0].env.att.source.sourceType = ctrlType_t::controller;
	envelopes[0].env.att.source.channel = 0;
	envelopes[0].env.att.source.sourceNum = 24;
	envelopes[0].env.dec.current = 0x0080'0000;
	envelopes[0].env.dec.max = 0x0800'0000;
	envelopes[0].env.dec.min = 0x0000'8000;
	envelopes[0].env.dec.source.sourceType = ctrlType_t::controller;
	envelopes[0].env.dec.source.channel = 0;
	envelopes[0].env.dec.source.sourceNum = 25;
	envelopes[0].env.sus.current = 0xA000;
	envelopes[0].env.sus.max = 0xffff;
	envelopes[0].env.sus.min = 0;
	envelopes[0].env.sus.source.sourceType = ctrlType_t::controller;
	envelopes[0].env.sus.source.channel = 0;
	envelopes[0].env.sus.source.sourceNum = 26;
	envelopes[0].env.rel.current = 0x1000'0000;
	envelopes[0].env.rel.max = 0x3fff'ffff;
	envelopes[0].env.rel.min = 1;
	envelopes[0].env.rel.source.sourceType = ctrlType_t::controller;
	envelopes[0].env.rel.source.channel = 0;
	envelopes[0].env.rel.source.sourceNum = 27;
}

void GO_Get_Config(ConfigNVM_t* conf){
	conf->bendRange = key_handler.get_bend_range();
	for (uint8_t i = 0; i < 4; i++){
		if (envelopes[i].env.att.source.sourceType == ctrlType_t::none){
			conf->env[i].att.max = uint32_to_ufloat8(envelopes[i].env.att.current);
			conf->env[i].att.min = uint32_to_ufloat8(envelopes[i].env.att.current);
		} else {
			conf->env[i].att.max = uint32_to_ufloat8(envelopes[i].env.att.max);
			conf->env[i].att.min = uint32_to_ufloat8(envelopes[i].env.att.min);
		}
		conf->env[i].att.source.channel = envelopes[i].env.att.source.channel;
		conf->env[i].att.source.sourceNum = envelopes[i].env.att.source.sourceNum;
		conf->env[i].att.source.sourceType = envelopes[i].env.att.source.sourceType;
		if (envelopes[i].env.dec.source.sourceType == ctrlType_t::none){
			conf->env[i].dec.max = uint32_to_ufloat8(envelopes[i].env.dec.current);
			conf->env[i].dec.min = uint32_to_ufloat8(envelopes[i].env.dec.current);
		} else {
			conf->env[i].dec.max = uint32_to_ufloat8(envelopes[i].env.dec.max);
			conf->env[i].dec.min = uint32_to_ufloat8(envelopes[i].env.dec.min);
		}
		conf->env[i].dec.source.channel = envelopes[i].env.dec.source.channel;
		conf->env[i].dec.source.sourceNum = envelopes[i].env.dec.source.sourceNum;
		conf->env[i].dec.source.sourceType = envelopes[i].env.dec.source.sourceType;
		if (envelopes[i].env.sus.source.sourceType == ctrlType_t::none){
			conf->env[i].sus.max = uint32_to_ufloat8(envelopes[i].env.sus.current);
			conf->env[i].sus.min = uint32_to_ufloat8(envelopes[i].env.sus.current);
		} else {
			conf->env[i].sus.max = uint32_to_ufloat8(envelopes[i].env.sus.max);
			conf->env[i].sus.min = uint32_to_ufloat8(envelopes[i].env.sus.min);
		}
		conf->env[i].sus.source.channel = envelopes[i].env.sus.source.channel;
		conf->env[i].sus.source.sourceNum = envelopes[i].env.sus.source.sourceNum;
		conf->env[i].sus.source.sourceType = envelopes[i].env.sus.source.sourceType;
		if (envelopes[i].env.rel.source.sourceType == ctrlType_t::none){
			conf->env[i].rel.max = uint32_to_ufloat8(envelopes[i].env.rel.current);
			conf->env[i].rel.min = uint32_to_ufloat8(envelopes[i].env.rel.current);
		} else {
			conf->env[i].rel.max = uint32_to_ufloat8(envelopes[i].env.rel.max);
			conf->env[i].rel.min = uint32_to_ufloat8(envelopes[i].env.rel.min);
		}
		conf->env[i].rel.source.channel = envelopes[i].env.rel.source.channel;
		conf->env[i].rel.source.sourceNum = envelopes[i].env.rel.source.sourceNum;
		conf->env[i].rel.source.sourceType = envelopes[i].env.rel.source.sourceType;
	}
	for (uint8_t i = 0; i < 16; i++){
		uint8_t x = i & 0b11;
		uint8_t y = i >> 2;
		conf->matrix[x][y].type = out_handler[x][y].state.type;
		conf->matrix[x][y].max_range = out_handler[x][y].state.max_range;
		conf->matrix[x][y].min_range = out_handler[x][y].state.min_range;
		conf->matrix[x][y].key_lane = out_handler[x][y].get_key_lane();
		conf->matrix[x][y].gen_source.channel = out_handler[x][y].state.gen_source.channel;
		conf->matrix[x][y].gen_source.sourceNum = out_handler[x][y].state.gen_source.sourceNum;
		conf->matrix[x][y].gen_source.sourceType = out_handler[x][y].state.gen_source.sourceType;
		switch (out_handler[x][y].state.type){
		case GOType_t::LFO:
		case GOType_t::CLK:
			conf->matrix[x][y].shape = out_handler[x][y].state.shape;
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::none){
				conf->matrix[x][y].freq_max = uint32_to_ufloat8(out_handler[x][y].state.freq_current);
				conf->matrix[x][y].freq_min = uint32_to_ufloat8(out_handler[x][y].state.freq_current);
			} else {
				conf->matrix[x][y].freq_max = uint32_to_ufloat8(out_handler[x][y].state.freq_max);
				conf->matrix[x][y].freq_min = uint32_to_ufloat8(out_handler[x][y].state.freq_min);
			}
			break;
		case GOType_t::Envelope:
			conf->matrix[x][y].env_num = out_handler[x][y].state.env_num;
			break;
		case GOType_t::DC:
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::none){
				conf->matrix[x][y].max_range = out_handler[x][y].state.currentOut;
				conf->matrix[x][y].min_range = out_handler[x][y].state.currentOut;
			} else {
				conf->matrix[x][y].max_range = out_handler[x][y].state.max_range;
				conf->matrix[x][y].min_range = out_handler[x][y].state.min_range;
			}
			break;
		default:
			break;
		}
	}
}

void GO_Set_Config(ConfigNVM_t* conf){
	key_handler.set_bend_range(conf->bendRange);
	for (uint8_t i = 0; i < 4; i++){
		uint32_t temp_max = ufloat8_to_uint32(conf->env[i].att.max);
		uint32_t temp_min = ufloat8_to_uint32(conf->env[i].att.min);
		envelopes[i].env.att.max = temp_max;
		envelopes[i].env.att.min = temp_min;
		envelopes[i].env.att.current = ((temp_max-temp_min) >> 1) + temp_min;
		envelopes[i].env.att.source.channel = conf->env[i].att.source.channel;
		envelopes[i].env.att.source.sourceNum = conf->env[i].att.source.sourceNum;
		envelopes[i].env.att.source.sourceType = conf->env[i].att.source.sourceType;
		temp_max = ufloat8_to_uint32(conf->env[i].dec.max);
		temp_min = ufloat8_to_uint32(conf->env[i].dec.min);
		envelopes[i].env.dec.max = temp_max;
		envelopes[i].env.dec.min = temp_min;
		envelopes[i].env.dec.current = ((temp_max-temp_min) >> 1) + temp_min;
		envelopes[i].env.dec.source.channel = conf->env[i].dec.source.channel;
		envelopes[i].env.dec.source.sourceNum = conf->env[i].dec.source.sourceNum;
		envelopes[i].env.dec.source.sourceType = conf->env[i].dec.source.sourceType;
		temp_max = ufloat8_to_uint32(conf->env[i].sus.max);
		temp_min = ufloat8_to_uint32(conf->env[i].sus.min);
		envelopes[i].env.sus.max = temp_max;
		envelopes[i].env.sus.min = temp_min;
		envelopes[i].env.sus.current = ((temp_max-temp_min) >> 1) + temp_min;
		envelopes[i].env.sus.source.channel = conf->env[i].sus.source.channel;
		envelopes[i].env.sus.source.sourceNum = conf->env[i].sus.source.sourceNum;
		envelopes[i].env.sus.source.sourceType = conf->env[i].sus.source.sourceType;
		temp_max = ufloat8_to_uint32(conf->env[i].rel.max);
		temp_min = ufloat8_to_uint32(conf->env[i].rel.min);
		envelopes[i].env.rel.max = temp_max;
		envelopes[i].env.rel.min = temp_min;
		envelopes[i].env.rel.current = ((temp_max-temp_min) >> 1) + temp_min;
		envelopes[i].env.rel.source.channel = conf->env[i].rel.source.channel;
		envelopes[i].env.rel.source.sourceNum = conf->env[i].rel.source.sourceNum;
		envelopes[i].env.rel.source.sourceType = conf->env[i].rel.source.sourceType;
	}
	for (uint8_t i = 0; i < 16; i++){
		uint8_t x = i & 0b11;
		uint8_t y = i >> 2;
		out_handler[x][y].set_type(conf->matrix[x][y].type);
		uint16_t rmax = conf->matrix[x][y].max_range;
		uint16_t rmin = conf->matrix[x][y].min_range;
		out_handler[x][y].state.max_range = rmax;
		out_handler[x][y].state.min_range = rmin;
		out_handler[x][y].state.key_lane = conf->matrix[x][y].key_lane;
		out_handler[x][y].state.gen_source.channel = conf->matrix[x][y].gen_source.channel;
		out_handler[x][y].state.gen_source.sourceNum = conf->matrix[x][y].gen_source.sourceNum;
		out_handler[x][y].state.gen_source.sourceType = conf->matrix[x][y].gen_source.sourceType;
		uint32_t fmax;
		uint32_t fmin;
		switch (out_handler[x][y].state.type){
		case GOType_t::Gate:
			out_handler[x][y].state.currentOut = rmin;
			break;
		case GOType_t::LFO:
		case GOType_t::CLK:
			out_handler[x][y].state.currentOut = rmin;
			out_handler[x][y].state.shape = conf->matrix[x][y].shape;
			fmax = ufloat8_to_uint32(conf->matrix[x][y].freq_max);
			fmin = ufloat8_to_uint32(conf->matrix[x][y].freq_min);
			out_handler[x][y].state.freq_max = fmax;
			out_handler[x][y].state.freq_min = fmin;
			if (out_handler[x][y].state.gen_source.sourceType == ctrlType_t::key) {
				out_handler[x][y].state.freq_current = FREQS.midi[60];
			} else {
				out_handler[x][y].state.freq_current = ((fmax-fmin)>>1) + fmin;
			}
			if (out_handler[x][y].state.shape == WavShape_t::Square){
				out_handler[x][y].state.direction = 1;
			} else {
				out_handler[x][y].state.direction = -1;
			}
			break;
		case GOType_t::Envelope:
			out_handler[x][y].state.currentOut = rmin;
			out_handler[x][y].state.env_num = conf->matrix[x][y].env_num;
			out_handler[x][y].state.envelope_stage = EnvStage_t::idle;
		default:
			out_handler[x][y].state.currentOut = ((rmax-rmin)>>1) + rmin;
			break;
		}
	}
	needScan = true;
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
	} else if (go->shape == WavShape_t::SuperSaw){
		// TODO
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
	} else if (go->shape == WavShape_t::Noise){
		// TODO: other colors?
		go->outCount -= go->freq_current;
		uint32_t remain = go->outCount;
		if (remain < go->freq_current){
			go->currentOut = Rescale_16bit(rp_rand.get(), go->min_range, go->max_range);
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
	env_handler_c* tempEnv = &envelopes[go->env_num];
	uint32_t remain;
	uint32_t tempSus;
	switch(go->envelope_stage){
		case EnvStage_t::attack:
			// attack
			remain = (0xFFFF'FFFF << 16) - go->outCount;
			if (remain <= tempEnv->get(EnvStage_t::attack)){
				go->envelope_stage = EnvStage_t::decay;
				go->outCount = 0xFFFF'FFFF << 16;
			} else {
				go->outCount += tempEnv->get(EnvStage_t::attack);
			}
			break;
		case EnvStage_t::decay:
			// decay
			tempSus = tempEnv->get(EnvStage_t::sustain) << 16;
			remain = go->outCount - tempSus;
			if (remain <= tempEnv->get(EnvStage_t::decay)){
				go->envelope_stage = EnvStage_t::sustain;
				go->outCount = tempSus;
			} else {
				go->outCount -= tempEnv->get(EnvStage_t::decay);
			}
			break;
		case EnvStage_t::release:
			// release
			if (go->outCount <= tempEnv->get(EnvStage_t::release)){
				go->envelope_stage = EnvStage_t::idle;
				go->outCount = 0;
			} else {
				go->outCount -= tempEnv->get(EnvStage_t::release);
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
	case PITCH_BEND_PERNOTE:
	case PITCH_BEND:
		// update v/oct outputs
		criteria = ( key_handler.get_key_channel() << 0 ) | ( uint8_t(ctrlType_t::key) << 8 );
		src_current = ( genout->gen_source.channel << 0 ) | ( uint8_t(genout->gen_source.sourceType) << 8 );
		if ( src_current == criteria ){
			genout->currentOut = Note_To_Output(genout->gen_source.sourceNum);
			genout->currentOut += key_handler.get_current_bend(genout->key_lane);
		}
		break;
	default:
		return;
	}
}

// TODO: use CC/NRPN lookup table
// TODO: handle RPN for poly modulation
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
		// TODO: allow detuning and sub-audible oscillators
		// TODO: handle range limiting
		if (msg->status == NOTE_ON){
			//genout->freq_current = FREQS.midi[msg->note];
			genout->gen_source.sourceNum = msg->note;
			if (genout->shape == WavShape_t::Noise){
				genout->currentOut = Rescale_16bit(rp_rand.get(), genout->min_range, genout->max_range);
			}
		} else if (msg->status == PITCH_BEND){
			// Bend must be taken account of at note start too
		}
		if (genout->shape == WavShape_t::Noise) return;
		// Calculate frequency
		int64_t tempBend = key_handler.get_current_bend();
		tempBend += key_handler.get_current_bend(genout->key_lane);
		tempBend *= FIXED_VOLT_PER_INT;	// int * fixed-point, no need to right-shift
		tempBend = fp_exp2(tempBend);
		tempBend *= FREQS.midi[genout->gen_source.sourceNum];
		tempBend >>= FIXED_POINT_POS;
		genout->freq_current = tempBend;
	}
}

void envelope_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	if (msg->status == NOTE_ON) {
		genout->gen_source.sourceNum = msg->note;
		//genout->outCount = tempOut->min_range << 16;
		genout->envelope_stage = EnvStage_t::attack;
	} else if (msg->status == NOTE_OFF) {
		genout->envelope_stage = EnvStage_t::release;
	}
}

// TODO: fix keylanes, maybe split handle cc from handle note and add a subscriber mechanism?
void pressure_output_c::handle_cvm(GenOut_t* genout, umpCVM* msg){
	if (msg->status == NOTE_ON){
		genout->gen_source.sourceNum = msg->note;
	} else if (msg->status == CHANNEL_PRESSURE){
	} else if (msg->status == KEY_PRESSURE){
		if (genout->gen_source.sourceNum != msg->note) {
			return;
		}
	} else {
		return;
	}
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
	current_bend = 0;
	for (uint8_t i = 0; i < 8; i++){
		num_outputs[i] = 0;
		key_playing[i] = -1;
		drum_note[i] = -1;
		bend_per_note[i] = 0;
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
				temp_out->envelope_stage = EnvStage_t::release;
			} else if (temp_out->type == GOType_t::Gate){
				temp_out->currentOut = temp_out->min_range;
			}
		}
	}
}

void key_handler_c::start_note(uint8_t lane, umpCVM* msg){
	key_playing[lane] = msg->note;
	bend_per_note[lane] = 0;
	for (uint8_t i = 0; i < num_outputs[lane]; i++){
		lanes[lane][i]->handle_cvm(msg);
	}

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
				com_out[j]->state.envelope_stage = EnvStage_t::attack;
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
	uint32_t span = (FIXED_INT_PER_NOTE * (range+1)) >> FIXED_POINT_POS;
	// Configure note bend range
	max_bend = 0x7fff + span;
	min_bend = 0x7fff - span;
}

uint8_t key_handler_c::get_bend_range(){
	uint32_t span = max_bend - 0x7fff;
	// Find the current bend range setting
	uint8_t i;
	for (i = 2; i <= 36; i++){
		if (span < ((i * FIXED_INT_PER_NOTE) >> FIXED_POINT_POS)){
			break;
		}
	}
	return i - 2;
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
				next_lane = 0;
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
			if (next_lane >= num_lanes) {
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
		if (channel == msg->channel) {
			// Update currentbend, all outputs will be updated after
			uint16_t tempBend = Rescale_16bit(msg->value >> 16, min_bend, max_bend);
			current_bend = tempBend - 0x7fff;
		} else {
			// Don't apply bend from other channels
			return 1;
		}
	} else if (msg->status == PITCH_BEND_PERNOTE){
		if (channel == msg->channel) {
			int8_t temp_lane = -1;
			for (uint8_t i = 0; i < num_lanes; i++){
				if (msg->note == key_playing[i]){
					temp_lane = i;
					break;
				}
			}
			if (temp_lane >= 0){
				uint16_t tempBend = Rescale_16bit(msg->value >> 16, min_bend, max_bend);
				bend_per_note[temp_lane] = tempBend - 0x7fff;
				for (uint8_t i = 0; i < num_outputs[temp_lane]; i++){
					lanes[temp_lane][i]->handle_cvm(msg);
				}
			}
		}
		return 1;
	} else if (msg->channel != channel) {
		// Don't apply pressure or from other channels
		if (msg->status == KEY_PRESSURE){
			return 1;
		} else if (msg->status == CHANNEL_PRESSURE) {
			return 1;
		}
	}
	return 0;
}

uint8_t key_handler_c::subscribe_key(generic_output_c* handler){
	return subscribe_key(handler, handler->get_key_lane());
}

uint8_t key_handler_c::subscribe_key(generic_output_c* handler, uint8_t lane){
	if (lane == 0){
		com_out[num_coms++] = handler;
	} else {
		if (num_outputs[lane-1] >= 4){
			return 0;
		}
		lanes[lane-1][num_outputs[lane-1]++] = handler;
		if (num_lanes < lane){
			num_lanes = lane;
		}
	}
	return 1;
}

uint8_t key_handler_c::subscribe_drum(generic_output_c* handler){
	uint8_t note = handler->state.gen_source.sourceNum;
	for (int8_t l = 7; l >= num_lanes; l--){
		if ( note == drum_note[l] ){
			lanes[l][num_outputs[l]++] = handler;
			break;
		} else if (drum_note[l] == -1){
			// New lane
			drum_note[l] = note;
			lanes[l][num_outputs[l]++] = handler;
			break;
		}
	}
}

void env_handler_c::handle_cvm(umpCVM* msg){
	// TODO: controlnum -> control lookup table
	uint16_t controlNum = msg->index;
	uint32_t criteria = ( uint8_t(ctrlType_t::controller) << 0 ) | ( msg->channel << 8 ) | ( controlNum << 16 );
	for (uint8_t i = 0; i < 4; i++) {
		Env_stage_t* stage;
		if (i == 0) stage = &env.att;
		else if (i == 1) stage = &env.dec;
		else if (i == 2) stage = &env.sus;
		else if (i == 3) stage = &env.rel;
		uint32_t src_current =
			( uint8_t(stage->source.sourceType) << 0 ) |
			( stage->source.channel << 8 ) |
			( stage->source.sourceNum << 16 );
		if (src_current == criteria){
			set_stage(msg->value, stage);
		}
	}
}

uint32_t env_handler_c::get(EnvStage_t stage){
	if (stage == EnvStage_t::attack) return env.att.current;
	else if (stage == EnvStage_t::decay) return env.dec.current;
	else if (stage == EnvStage_t::sustain) return env.sus.current;
	else if (stage == EnvStage_t::release) return env.rel.current;
	return 0;
}

// TODO: fix scaling
void env_handler_c::set_stage(uint32_t val, Env_stage_t* stage){
	if (stage->max > stage->min){
		uint32_t diff = stage->max - stage->min;
		uint32_t span = diff * 0x0101 + 1;
		uint32_t scaled = (val >> 16) * span;
		stage->current = 0x0101 * stage->min + (scaled >> 16);
	} else {
		uint32_t diff = stage->min - stage->max;
		uint32_t span = diff * 0x0101 + 1;
		uint32_t scaled = (val >> 16) * span;
		stage->current = 0x0101 * stage->min - (scaled >> 16);
	}
}

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

	if (msg->status != CC) {
		// Envelope handler only deal with CC
		return;
	}


	// Search envelopes
	for (uint8_t i = 0; i < 4; i++){
		envelopes[i].handle_cvm(msg);
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

