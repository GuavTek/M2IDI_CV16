/*
 * menu.cpp
 *
 * Created: 19/07/2021 17:01:37
 *  Author: GuavTek
 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include <hardware/timer.h>
#include "umpProcessor.h"
#include "utils.h"
#include "led_matrix.h"
#include "generic_output.h"
#include "menu.h"

menu_status_t menuStatus;
menuNode* currentNode;
void* var_edit;
void* var_monitor;
uint8_t max_edit;
uint8_t bit_edit_stage = 0;
uint8_t chanSel = 0;
volatile bool buttUp;
volatile bool buttDown;
volatile bool buttRight;

// i0: CC, i1: Key, i2: PC
uint8_t midiTypeMask;	

const uint8_t MAX_SLOTS = 8;
bool needSave = false;
bool needLoad = false;
uint8_t confSlot;
ctrlSource_t confPC;
uint8_t butt_state;
uint8_t butt_state_debounced;
uint32_t butt_timer;
const uint32_t butt_timeout = 20000;	// 20ms timeout

inline uint32_t extrapolate_num (uint32_t in, uint8_t pos){
	uint32_t tempResult = in & (0xffff'ffff << pos);
	uint8_t cleanIn = (in >> pos) & 0xf;
	int8_t i = pos - 4;
	for (; i > 0; i -= 4){
		tempResult |= cleanIn << i;
	}
	tempResult |= cleanIn >> -i;
	return tempResult;
}

inline uint16_t extrapolate_num (uint16_t in, uint8_t pos){
	uint16_t tempResult = in & (0xffff << pos);
	uint8_t cleanIn = (in >> pos) & 0xf;
	int8_t i = pos - 4;
	for (; i > 0; i -= 4){
		tempResult |= cleanIn << i;
	}
	tempResult |= cleanIn >> -i;
	return tempResult;
}

inline uint8_t extrapolate_num (uint8_t in, uint8_t pos){
	return in | (in >> 4);
}

extern struct menuNode edit_n;
extern struct menuNode edit_bend_n;
extern struct menuNode edit_select_n;
extern struct menuNode edit_sel_back_n;
extern struct menuNode edit_sel_bind_n;
extern struct menuNode edit_sel_type_n;
extern struct menuNode edit_sel_type_pressure_n;
extern struct menuNode edit_sel_type_CV_n;
extern struct menuNode edit_sel_type_gate_n;
extern struct menuNode edit_sel_type_envelope_n;
extern struct menuNode edit_sel_type_velocity_n;
extern struct menuNode edit_sel_type_clk_n;
extern struct menuNode edit_sel_type_lfo_n;
extern struct menuNode edit_sel_type_lfo_shape_n;
extern struct menuNode edit_sel_type_lfo_fmax_n;
extern struct menuNode edit_sel_type_lfo_fmin_n;
extern struct menuNode edit_sel_type_lfo_back_n;
extern struct menuNode edit_sel_type_back_n;
extern struct menuNode edit_sel_max_range_n;
extern struct menuNode edit_sel_min_range_n;
extern struct menuNode edit_envelope;
extern struct menuNode edit_env_0;
extern struct menuNode edit_env_1;
extern struct menuNode edit_env_2;
extern struct menuNode edit_env_3;
extern struct menuNode edit_env_back;
extern struct menuNode edit_env_atk_n;
extern struct menuNode edit_env_atk_max_n;
extern struct menuNode edit_env_atk_min_n;
extern struct menuNode edit_env_atk_bind_n;
extern struct menuNode edit_env_atk_back_n;
extern struct menuNode edit_env_dec_n;
extern struct menuNode edit_env_dec_max_n;
extern struct menuNode edit_env_dec_min_n;
extern struct menuNode edit_env_dec_bind_n;
extern struct menuNode edit_env_dec_back_n;
extern struct menuNode edit_env_sus_n;
extern struct menuNode edit_env_sus_max_n;
extern struct menuNode edit_env_sus_min_n;
extern struct menuNode edit_env_sus_bind_n;
extern struct menuNode edit_env_sus_back_n;
extern struct menuNode edit_env_rel_n;
extern struct menuNode edit_env_rel_max_n;
extern struct menuNode edit_env_rel_min_n;
extern struct menuNode edit_env_rel_bind_n;
extern struct menuNode edit_env_rel_back_n;
extern struct menuNode edit_env_back_n;
extern struct menuNode edit_back_n;
extern struct menuNode save_n;
extern struct menuNode save_slot_n;
extern struct menuNode save_bind_pc_n;
extern struct menuNode save_back_n;
extern struct menuNode save_back_accept_n;
extern struct menuNode save_back_abort_n;
extern struct menuNode load_n;
extern struct menuNode load_slot_n;
extern struct menuNode load_back_n;
extern struct menuNode edit_group;

void Enter_Kid()			{ currentNode = currentNode->kid; }
void Enter_Env0()			{ chanSel = 0; Enter_Kid(); }
void Enter_Env1()			{ chanSel = 1; Enter_Kid(); }
void Enter_Env2()			{ chanSel = 2; Enter_Kid(); }
void Enter_Env3()			{ chanSel = 3; Enter_Kid(); }
void Exit_Env()			{ chanSel = 0; Enter_Kid(); }
void Edit_Bend()			{ menuStatus = Edit_int; var_edit = &bendRange; max_edit = 12; }
void Select_Pressure()	{ out_handler[chanSel & 0b11][chanSel >> 2].set_type(GOType_t::Pressure); needScan = true; Enter_Kid(); }
void Select_CV()			{ out_handler[chanSel & 0b11][chanSel >> 2].set_type(GOType_t::DC); needScan = true; Enter_Kid(); }
void Select_Gate()		{ out_handler[chanSel & 0b11][chanSel >> 2].set_type(GOType_t::Gate); needScan = true; Enter_Kid(); }
void Select_Envelope()	{ menuStatus = Edit_int; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.env_num; max_edit = 4; out_handler[chanSel & 0b11][chanSel >> 2].set_type(GOType_t::Envelope); }
void Select_Velocity()	{ out_handler[chanSel & 0b11][chanSel >> 2].set_type(GOType_t::Velocity); needScan = true; Enter_Kid(); }
void Select_Clk()			{ menuStatus = Edit_int; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.freq_current; max_edit = 128; out_handler[chanSel & 0b11][chanSel >> 2].set_type(GOType_t::CLK); }
void Select_LFO()			{ out_handler[chanSel & 0b11][chanSel >> 2].set_type(GOType_t::LFO); needScan = true; Enter_Kid(); }
void Select_LFO_Shape()	{ menuStatus = SetLFO; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.shape; }
void Select_LFO_Freq_Max(){ menuStatus = Edit_32bit; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.freq_max; var_monitor = &out_handler[chanSel & 0b11][chanSel >> 2].state.freq_current; }
void Select_LFO_Freq_Min(){ menuStatus = Edit_32bit; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.freq_min; var_monitor = &out_handler[chanSel & 0b11][chanSel >> 2].state.freq_current; }
void Set_GO_MIDI()		{ menuStatus = Wait_MIDI; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.gen_source; midiTypeMask = 0b111; }
void Set_Max_Range()		{ menuStatus = Edit_16bit; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.max_range; var_monitor = &out_handler[chanSel & 0b11][chanSel >> 2].state.currentOut; }
void Set_Min_Range()		{ menuStatus = Edit_16bit; var_edit = &out_handler[chanSel & 0b11][chanSel >> 2].state.min_range; var_monitor = &out_handler[chanSel & 0b11][chanSel >> 2].state.currentOut; }
void Set_Conf_Slot()		{ menuStatus = Edit_int; var_edit = &confSlot; max_edit = MAX_SLOTS; }
void Set_Save_PC()		{ menuStatus = Wait_MIDI; var_edit = &confPC; midiTypeMask = 0b100; }
void Save_Config()		{ needSave = true; Enter_Kid(); }
void Load_Config()		{ needLoad = true; Enter_Kid(); }
void Set_Group()			{ menuStatus = Edit_int; var_edit = &midi_group; max_edit = 16; }
void Set_Env_Atk_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].att_max; var_monitor = &envelopes[chanSel].att_current; }
void Set_Env_Atk_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].att_min; var_monitor = &envelopes[chanSel].att_current; }
void Set_Env_Atk_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].att_source; midiTypeMask = 0b001; }
void Set_Env_Dec_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].dec_max; var_monitor = &envelopes[chanSel].dec_current; }
void Set_Env_Dec_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].dec_min; var_monitor = &envelopes[chanSel].dec_current; }
void Set_Env_Dec_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].dec_source; midiTypeMask = 0b001; }
void Set_Env_Sus_Max()	{ menuStatus = Edit_16bit; var_edit = &envelopes[chanSel].sus_max; var_monitor = &envelopes[chanSel].sus_current; }
void Set_Env_Sus_Min()	{ menuStatus = Edit_16bit; var_edit = &envelopes[chanSel].sus_min; var_monitor = &envelopes[chanSel].sus_current; }
void Set_Env_Sus_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].sus_source; midiTypeMask = 0b001; }
void Set_Env_Rel_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].rel_max; var_monitor = &envelopes[chanSel].rel_current; }
void Set_Env_Rel_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].rel_min; var_monitor = &envelopes[chanSel].rel_current; }
void Set_Env_Rel_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].rel_source; midiTypeMask = 0b001; }

// Used to bind MIDI sources in configuration
uint8_t menu_midi(struct umpCVM* msg){
	if (menuStatus == menu_status_t::Wait_MIDI){
		ctrlSource_t* tempSource = (ctrlSource_t*) var_edit;
		
		switch(msg->status){
			case PROGRAM_CHANGE:
				if (!(midiTypeMask & 0b100)){
					return 0;
				}
				// TODO: Set global bank?
				tempSource->sourceType = ctrlType_t::program;
				tempSource->sourceNum = msg->value;
				break;
			case NOTE_ON:
			case NOTE_OFF:
				if (!(midiTypeMask & 0b010)){
					return 0;
				}
				tempSource->sourceType = ctrlType_t::key;
				tempSource->sourceNum = msg->note;
				break;
			case CC:
				if (!(midiTypeMask & 0b001)){
					return 0;
				}
				tempSource->sourceType = ctrlType_t::controller;
				tempSource->sourceNum = msg->index;
				break;
			default:
				return 0;
		}
		
		tempSource->channel = msg->channel;
		menuStatus = menu_status_t::Navigate;
		needScan = true;
		Enter_Kid();
		return 1;
	}
	return 0;
}

void menu_init(){
	currentNode = &edit_n;
	menuStatus = Navigate;
	
	// Enable pin interrupts on buttons
	gpio_init(BUTT1);
	gpio_init(BUTT2);
	gpio_init(BUTT3);
	gpio_pull_up(BUTT1);
	gpio_pull_up(BUTT2);
	gpio_pull_up(BUTT3);
}

void check_buttons(){
	uint8_t temp_state;
	temp_state = gpio_get(BUTT1) | (gpio_get(BUTT2) << 1) | (gpio_get(BUTT3) << 2);
	if (temp_state == butt_state) {
		// inputs are debounced
		uint8_t butt_edge = butt_state_debounced & ~temp_state;
		if (butt_edge & 0b001){
			// BUTT1
			buttRight = true;
		}
		if (butt_edge & 0b010){
			// BUTT2
			buttUp = true;
		}
		if (butt_edge & 0b100){
			// BUTT3
			buttDown = true;
		}
		butt_state_debounced = temp_state;
	}
	butt_timer = time_us_32() + butt_timeout;
	butt_state = temp_state;
}

uint8_t menu_service(){
	bool screenChange = false;
	if (time_us_32() > butt_timer) {
		check_buttons();
	}
	switch(menuStatus){
		case menu_status_t::Navigate:
			if (currentNode == &edit_select_n){
				if (buttUp) {
					if (chanSel > 0){
						chanSel--;
					} else {
						currentNode = currentNode->previous;
					}
					buttUp = false;
					screenChange = true;
				}
				if (buttDown) {
					if (chanSel < 15){
						chanSel++;
					} else {
						currentNode = currentNode->next;
						chanSel = 0;
					}
					buttDown = false;
					screenChange = true;
				}
				
				if (buttRight) {
					currentNode->function();
					buttRight = false;
					screenChange = true;
				}
			} else {
				if (buttUp) {
					currentNode = currentNode->previous;
					buttUp = false;
					screenChange = true;
				}
				if (buttDown) {
					currentNode = currentNode->next;
					buttDown = false;
					screenChange = true;
				}
				if (buttRight) {
					currentNode->function();
					buttRight = false;
					screenChange = true;
				}
			}
			break;
		case menu_status_t::Wait_MIDI:
			// Press to un-bind
			if (buttRight){
				buttRight = false;
				ctrlSource_t* tempSrc = (ctrlSource_t*) var_edit;
				tempSrc->sourceType = ctrlType_t::none;
				menuStatus = menu_status_t::Navigate;
				needScan = true;
				Enter_Kid();
			}
			if (buttDown){
				buttDown = false;
			}
			if (buttUp){
				buttUp = false;
			}
			screenChange = true;
			break;
		case menu_status_t::SetLFO:
			if (buttUp) {
				uint8_t* tempPoint = (uint8_t*) var_edit;
				uint8_t tempShape = *tempPoint;
				tempShape++;
				if (tempShape >= 5){
					tempShape = 0;
				}
				*tempPoint = tempShape;
				buttUp = false;
			}
			if (buttDown) {
				uint8_t* tempPoint = (uint8_t*) var_edit;
				uint8_t tempShape = *tempPoint;
				tempShape--;
				if (tempShape >= 5){
					tempShape = 4;
				}
				*tempPoint = tempShape;
				buttDown = false;
			}
			if (buttRight) {
				buttRight = false;
				menuStatus = menu_status_t::Navigate;
				Enter_Kid();
			}
			screenChange = true;
			break;
		case menu_status_t::Edit_int:
			if (buttUp) {
				uint8_t* tempPoint = (uint8_t*) var_edit;
				uint8_t tempInt = *tempPoint;
				tempInt++;
				if (tempInt >= max_edit){
					tempInt = 0;
				}
				*tempPoint = tempInt;
				buttUp = false;
				needScan = true;
			}
			if (buttDown) {
				uint8_t* tempPoint = (uint8_t*) var_edit;
				uint8_t tempInt = *tempPoint;
				tempInt--;
				if (tempInt >= max_edit){
					tempInt = max_edit - 1;
				}
				*tempPoint = tempInt;
				buttDown = false;
				needScan = true;
			}
			if (buttRight) {
				buttRight = false;
				menuStatus = menu_status_t::Navigate;
				needScan = true;
				Enter_Kid();
			}
			screenChange = true;
			break;
		case menu_status_t::Edit_8bit:
			if (buttUp) {
				uint8_t* tempPoint = (uint8_t*) var_edit;
				uint8_t tempInt = *tempPoint;
				buttUp = false;
				needScan = true;
				if (bit_edit_stage & 0x80){
					uint8_t delta = 1 << (bit_edit_stage & 0x07);
					tempInt += delta;
					tempInt = extrapolate_num(tempInt, bit_edit_stage & 0x07);
				} else {
					uint8_t i;
					for (i = 7; (i > 0) && !(tempInt & (1 << i)); i--);
					i++;
					bit_edit_stage = i & 0x07;
					tempInt = 1 << bit_edit_stage;
				}
				*tempPoint = tempInt;
				tempPoint = (uint8_t*) var_monitor;
				*tempPoint = tempInt;
			}
			if (buttDown) {
				uint8_t* tempPoint = (uint8_t*) var_edit;
				uint8_t tempInt = *tempPoint;
				buttDown = false;
				needScan = true;
				if (bit_edit_stage & 0x80){
					uint8_t delta = 1 << (bit_edit_stage & 0x07);
					tempInt -= delta;
					tempInt = extrapolate_num(tempInt, bit_edit_stage & 0x07);
				} else {
					uint8_t i;
					for (i = 7; (i > 0) && !(tempInt & (1 << i)); i--);
					i--;
					bit_edit_stage = i & 0x07;
					tempInt = 1 << bit_edit_stage;
				}
				*tempPoint = tempInt;
				tempPoint = (uint8_t*) var_monitor;
				*tempPoint = tempInt;
			}
			if (buttRight) {
				buttRight = false;
				if (bit_edit_stage & 0x80){
					bit_edit_stage = 0;
					menuStatus = menu_status_t::Navigate;
					needScan = true;
					Enter_Kid();
				} else {
					if (bit_edit_stage > 3){
						bit_edit_stage -= 3;
					} else {
						bit_edit_stage = 0;
					}
					bit_edit_stage |= 0x80;
				}
			}
			screenChange = true;
			break;
		case menu_status_t::Edit_16bit:
			if (buttUp) {
				uint16_t* tempPoint = (uint16_t*) var_edit;
				uint16_t tempInt = *tempPoint;
				buttUp = false;
				needScan = true;
				if (bit_edit_stage & 0x80){
					uint16_t delta = 1 << (bit_edit_stage & 0x0f);
					tempInt += delta;
					tempInt = extrapolate_num(tempInt, bit_edit_stage & 0x0f);
				} else {
					uint8_t i;
					for (i = 15; (i > 0) && !(tempInt & (1 << i)); i--);
					i++;
					bit_edit_stage = i & 0x0f;
					tempInt = 1 << bit_edit_stage;
				}
				*tempPoint = tempInt;
				tempPoint = (uint16_t*) var_monitor;
				*tempPoint = tempInt;
			}
			if (buttDown) {
				uint16_t* tempPoint = (uint16_t*) var_edit;
				uint16_t tempInt = *tempPoint;
				buttDown = false;
				needScan = true;
				if (bit_edit_stage & 0x80){
					uint16_t delta = 1 << (bit_edit_stage & 0x0f);
					tempInt -= delta;
					tempInt = extrapolate_num(tempInt, bit_edit_stage & 0x0f);
				} else {
					uint8_t i;
					for (i = 15; (i > 0) && !(tempInt & (1 << i)); i--);
					i--;
					bit_edit_stage = i & 0x0f;
					tempInt = 1 << bit_edit_stage;
				}
				*tempPoint = tempInt;
				tempPoint = (uint16_t*) var_monitor;
				*tempPoint = tempInt;
			}
			if (buttRight) {
				buttRight = false;
				if (bit_edit_stage & 0x80){
					bit_edit_stage = 0;
					menuStatus = menu_status_t::Navigate;
					needScan = true;
					Enter_Kid();
				} else {
					if (bit_edit_stage > 3){
						bit_edit_stage -= 3;
					} else {
						bit_edit_stage = 0;
					}
					bit_edit_stage |= 0x80;
				}
			}
			screenChange = true;
			break;
		case menu_status_t::Edit_32bit:
			if (buttUp) {
				uint32_t* tempPoint = (uint32_t*) var_edit;
				uint32_t tempInt = *tempPoint;
				buttUp = false;
				needScan = true;
				if (bit_edit_stage & 0x80){
					uint32_t delta = 1 << (bit_edit_stage & 0x1f);
					tempInt += delta;
					tempInt = extrapolate_num(tempInt, bit_edit_stage & 0x1f);
				} else {
					uint8_t i;
					for (i = 31; (i > 0) && !(tempInt & (1 << i)); i--);
					i++;
					bit_edit_stage = i & 0x1f;
					tempInt = 1 << bit_edit_stage;
				}
				*tempPoint = tempInt;
				tempPoint = (uint32_t*) var_monitor;
				*tempPoint = tempInt;
			}
			if (buttDown) {
				uint32_t* tempPoint = (uint32_t*) var_edit;
				uint32_t tempInt = *tempPoint;
				buttDown = false;
				needScan = true;
				if (bit_edit_stage & 0x80){
					uint32_t delta = 1 << (bit_edit_stage & 0x1f);
					tempInt -= delta;
					tempInt = extrapolate_num(tempInt, bit_edit_stage & 0x1f);
				} else {
					uint8_t i;
					for (i = 31; (i > 0) && !(tempInt & (1 << i)); i--);
					i--;
					bit_edit_stage = i & 0x1f;
					tempInt = 1 << bit_edit_stage;
				}
				*tempPoint = tempInt;
				tempPoint = (uint32_t*) var_monitor;
				*tempPoint = tempInt;
			}
			if (buttRight) {
				buttRight = false;
				if (bit_edit_stage & 0x80){
					bit_edit_stage = 0;
					menuStatus = menu_status_t::Navigate;
					needScan = true;
					Enter_Kid();
				} else {
					if (bit_edit_stage > 3){
						bit_edit_stage -= 3;
					} else {
						bit_edit_stage = 0;
					}
					bit_edit_stage |= 0x80;
				}
			}
			screenChange = true;
			break;
		default:
			// Invalid state, reset menu
			menu_init();
			break;
	}
	
	if (screenChange) {
		if (menuStatus == menu_status_t::Navigate){
			// Channel select screen override
			if (currentNode == &edit_select_n){
				LM_WriteRow(4, 0xffff);
				for (uint8_t i = 0; i < 4; i++){
					LM_WriteRow(i, 0b1111000000001111 | (((chanSel % 4) == i) ? 0b10 << (2*(5 - chanSel/4)) : 0));
				}
			} else {
				for (uint8_t i = 0; i < 5; i++)	{
					LM_WriteRow(i, currentNode->graphic[i]);
				}
			}
		} else if (menuStatus == menu_status_t::Wait_MIDI){
			LM_WriteRow(0,0xffff);
			LM_WriteRow(1,0);
			LM_WriteRow(2,0);
			LM_WriteRow(4,0);
			uint8_t timer = (time_us_32() >> 18) & 0b1111;
			LM_WriteRow(3, 0b0011001100110011 & ~(0b11 << timer));
		} else if (menuStatus == menu_status_t::SetLFO){
			WavShape_t* tempPoint = (WavShape_t*) var_edit;
			WavShape_t tempShape = *tempPoint;
			switch(tempShape){
				case WavShape_t::Square:
					LM_WriteRow(0, 0b0011111111000011);
					LM_WriteRow(1, 0b0011000011000011);
					LM_WriteRow(2, 0b0011000011000011);
					LM_WriteRow(3, 0b0011000011000011);
					LM_WriteRow(4, 0b1111000011111111);
					break;
				case WavShape_t::Triangle:
					LM_WriteRow(0, 0b0000000011000000);
					LM_WriteRow(1, 0b0000001100110000);
					LM_WriteRow(2, 0b0000110000001100);
					LM_WriteRow(3, 0b0011000000000011);
					LM_WriteRow(4, 0b1100000000000000);
					break;
				case WavShape_t::Sawtooth:
					LM_WriteRow(0, 0b1100000011000000);
					LM_WriteRow(1, 0b1111000011110000);
					LM_WriteRow(2, 0b1100110011001100);
					LM_WriteRow(3, 0b1100001111000011);
					LM_WriteRow(4, 0b1100000011000000);
					break;
				case WavShape_t::Sine:
					LM_WriteRow(0, 0b0000000000000011);
					LM_WriteRow(1, 0b0000000000111100);
					LM_WriteRow(2, 0b0000000000110000);
					LM_WriteRow(3, 0b1100000011110000);
					LM_WriteRow(4, 0b0011111100000000);
					break;
				case WavShape_t::SinSaw:
					LM_WriteRow(0, 0b1111000000111100);
					LM_WriteRow(1, 0b1100110000110011);
					LM_WriteRow(2, 0b1100110000110000);
					LM_WriteRow(3, 0b1100001100110000);
					LM_WriteRow(4, 0b1100000011110000);
					break;
			}
		} else if (menuStatus == menu_status_t::Edit_int){
			uint8_t* tempPoint = (uint8_t*) var_edit;
			uint8_t tempNum = *tempPoint;
			uint8_t lo = tempNum & 0b111;
			uint8_t mid = (tempNum >> 3) & 0b111;
			uint8_t hi = tempNum >> 6;
			LM_WriteRow(0, 0xffff);
			LM_WriteRow(1, 0);
			LM_WriteRow(2, 0xffff >> (16-2*hi));
			LM_WriteRow(3, 0xffff >> (16-2*mid));
			LM_WriteRow(4, 0xffff >> (14-2*lo));
		} else {
			uint16_t dispNum[4];
			if (menuStatus == menu_status_t::Edit_8bit){
				uint8_t* tempPoint = (uint8_t*) var_edit;
				uint8_t tempExpand = *tempPoint;
				dispNum[0] = 0;
				for (uint8_t n = 0; n < 16; n += 2){
					uint8_t temp = tempExpand & 1;
					temp |= temp << 1;
					tempExpand >>= 1;
					dispNum[0] |= temp << n;
				}
				dispNum[1] = dispNum[0];
				dispNum[2] = dispNum[0];
				dispNum[3] = dispNum[0];
			} else if (menuStatus == menu_status_t::Edit_16bit){
				uint16_t* tempPoint = (uint16_t*) var_edit;
				uint16_t tempExpand = *tempPoint;
				for (uint8_t i = 0; i < 2; i++){
					dispNum[i] = 0;
					for (uint8_t n = 0; n < 16; n += 2){
						uint8_t temp = tempExpand & 1;
						temp |= temp << 1;
						tempExpand >>= 1;
						dispNum[i] |= temp << n;
					}
				}
				dispNum[2] = dispNum[0];
				dispNum[3] = dispNum[1];
			} else {
				uint32_t* tempPoint = (uint32_t*) var_edit;
				uint32_t tempExpand = *tempPoint;
				for (uint8_t i = 0; i < 4; i++){
					dispNum[i] = 0;
					for (uint8_t n = 0; n < 16; n += 2){
						uint8_t temp = tempExpand & 1;
						temp |= temp << 1;
						tempExpand >>= 1;
						dispNum[i] |= temp << n;
					}
				}
			}
			LM_WriteRow(4, 0xffff);
			LM_WriteRow(3, dispNum[0]);
			LM_WriteRow(2, dispNum[1]);
			LM_WriteRow(1, dispNum[2]);
			LM_WriteRow(0, dispNum[3]);
		}
		return 1;
	} else {
		return 0;
	}
}

menu_status_t get_menu_state(){
	return menuStatus;
}

// TODO: Improve node linking ( going down a level should lead to the first node at that level )

// lvl0
struct menuNode edit_n = {
	graphic :{	0b1111110011110000, 
				0b1100000011001100, 
				0b1111000011001100, 
				0b1100000011001100, 
				0b1111110011110000},
	kid :		&edit_bend_n,
	previous :	&load_n,
	next :		&save_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode edit_bend_n = {
	graphic :{	0b1111000011000011, 
				0b1100110011110011, 
				0b1111000011110011, 
				0b1100110011001111, 
				0b1111000011000011},
	kid :		&edit_n,
	previous :	&edit_back_n,
	next :		&edit_group,
	function :	Edit_Bend
};

struct menuNode edit_group = {
	graphic :{	0b1111001111001111,
				0b1111001111001111,
				0b0000000000000000,
				0b0101001111001111,
				0b0101001111001111},
	kid :		&edit_group,
	previous :	&edit_bend_n,
	next :		&edit_select_n,
	function :	Set_Group
};

struct menuNode edit_select_n = {
	graphic :{	0b1111111111111111, 
				0b1111110000001111, 
				0b1111000000001111, 
				0b1111000000001111, 
				0b1111000000001111},
	kid :		&edit_sel_type_n,
	previous :	&edit_group,
	next :		&edit_envelope,
	function :	Enter_Kid
};

// lvl2
struct menuNode edit_sel_max_range_n = {
	graphic :{	0b1111000000110000,
				0b1100110011111100,
				0b1111001100110011,
				0b1100110000110000,
				0b1100110000110000},
	kid :		&edit_sel_max_range_n,
	previous :	&edit_sel_back_n,
	next :		&edit_sel_min_range_n,
	function :	Set_Max_Range
};

struct menuNode edit_sel_min_range_n = {
	graphic :{	0b1111000000110000,
				0b1100110000110000,
				0b1111001100110011,
				0b1100110011111100,
				0b1100110000110000},
	kid :		&edit_sel_min_range_n,
	previous :	&edit_sel_max_range_n,
	next :		&edit_sel_bind_n,
	function :	Set_Min_Range
};

struct menuNode edit_sel_bind_n = {
	graphic :{	0b1100000011110000,
				0b1100000011001100,
				0b1100000011110000,
				0b1100000011001100,
				0b1111110011001100},
	kid :		&edit_sel_bind_n,
	previous :	&edit_sel_min_range_n,
	next :		&edit_sel_type_n,
	function :	Set_GO_MIDI
};

struct menuNode edit_sel_type_n = {
	graphic :{	0b1111110011110000,
				0b0011000011001100,
				0b0011000011110000,
				0b0011000011000000,
				0b0011000011000000},
	kid :		&edit_sel_type_CV_n,
	previous :	&edit_sel_bind_n,
	next :		&edit_sel_back_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_sel_type_CV_n = {
	graphic :{	0b0011110011001100,
				0b1100000011001100,
				0b1100000011001100,
				0b1100000000110000,
				0b0011110000110000},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_back_n,
	next :		&edit_sel_type_gate_n,
	function :	Select_CV
};

struct menuNode edit_sel_type_gate_n = {
	graphic :{	0b0011110000111111,
				0b1100000000001100,
				0b1100111100001100,
				0b1100001100001100,
				0b0011110000001100},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_CV_n,
	next :		&edit_sel_type_envelope_n,
	function :	Select_Gate
};

struct menuNode edit_sel_type_envelope_n = {
	graphic :{	0b0000110000000000,
				0b0011001100000000,
				0b0011000011110000,
				0b1100000000001100,
				0b1100000000000011},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_gate_n,
	next :		&edit_sel_type_lfo_n,
	function :	Select_Envelope
};

struct menuNode edit_sel_type_lfo_n = {
	graphic :{	0b0011111100111111,
				0b0011001100110011,
				0b0011001100110011,
				0b0011001100110011,
				0b1111001111110011},
	kid :		&edit_sel_type_lfo_shape_n,
	previous :	&edit_sel_type_envelope_n,
	next :		&edit_sel_type_clk_n,
	function :	Select_LFO
};

// lvl4
struct menuNode edit_sel_type_lfo_shape_n = {
	graphic :{	0b1111000000110000,
				0b0011000011001100,
				0b0011001100000011,
				0b0011110000000000,
				0b0011000000000000},
	kid :		&edit_sel_type_lfo_fmax_n,
	previous :	&edit_sel_type_lfo_back_n,
	next :		&edit_sel_type_lfo_fmax_n,
	function :	Select_LFO_Shape
};

struct menuNode edit_sel_type_lfo_fmax_n = {
	graphic :{	0b1111110000110000,
				0b1100000011111100,
				0b1111001100110011,
				0b1100000000110000,
				0b1100000000110000},
	kid :		&edit_sel_type_lfo_fmin_n,
	previous :	&edit_sel_type_lfo_shape_n,
	next :		&edit_sel_type_lfo_fmin_n,
	function :	Select_LFO_Freq_Max
};

struct menuNode edit_sel_type_lfo_fmin_n = {
	graphic :{	0b1111110000110000,
				0b1100000000110000,
				0b1111001100110011,
				0b1100000011111100,
				0b1100000000110000},
	kid :		&edit_sel_type_lfo_back_n,
	previous :	&edit_sel_type_lfo_fmax_n,
	next :		&edit_sel_type_lfo_back_n,
	function :	Select_LFO_Freq_Min
};

struct menuNode edit_sel_type_lfo_back_n = {
	graphic :{	0b0000000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b1100000011000000,
				0b0000000011111111},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_lfo_fmin_n,
	next :		&edit_sel_type_lfo_shape_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_sel_type_clk_n = {
	graphic :{	0b0011111100110011,
				0b1100001100110011,
				0b1100001100111100,
				0b1100001100110011,
				0b0011111111110011},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_lfo_n,
	next :		&edit_sel_type_velocity_n,
	function :	Select_Clk
};

struct menuNode edit_sel_type_velocity_n = {
	graphic :{	0b1100111111001100,
				0b1100111100001100,
				0b1100111111001100,
				0b0011001100001100,
				0b0011001111001111},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_clk_n,
	next :		&edit_sel_type_pressure_n,
	function :	Select_Velocity
};

struct menuNode edit_sel_type_pressure_n = {
	graphic :{	0b1111001111000011,
				0b1100111100111100,
				0b1111001111000011,
				0b1100001100110011,
				0b1100001100111100},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_velocity_n,
	next :		&edit_sel_type_back_n,
	function :	Select_Pressure
};

struct menuNode edit_sel_type_back_n = {
	graphic :{	0b0000000011000000,
				0b0000001111110000,
				0b1100110011001100,
				0b0000000011000000,
				0b0000000011111111},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_pressure_n,
	next :		&edit_sel_type_CV_n,
	function :	Enter_Kid
};

// lvl2
struct menuNode edit_sel_back_n = {
	graphic :{	0b0000000011000000,
				0b1100001111110000,
				0b0000110011001100,
				0b0000000011000000,
				0b0000000011111111},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_n,
	next :		&edit_sel_max_range_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode edit_envelope = {
	graphic :{	0b0000110000000000,
				0b0011001111000000,
				0b0011000000110000,
				0b1100000000001100,
				0b1100000000000011},
	kid :		&edit_env_0,
	previous :	&edit_select_n,
	next :		&edit_back_n,
	function :	Enter_Kid
};

// lvl2
struct menuNode edit_env_0 = {
	graphic :{	0b1111110000001100,
				0b1100000000110011,
				0b1111000000110011,
				0b1100000000110011,
				0b1111110000001100},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_back,
	next :		&edit_env_1,
	function :	Enter_Env0
};

struct menuNode edit_env_1 = {
	graphic :{	0b1111110000001100,
				0b1100000000001100,
				0b1111000000001100,
				0b1100000000001100,
				0b1111110000001100},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_0,
	next :		&edit_env_2,
	function :	Enter_Env1
};

struct menuNode edit_env_2 = {
	graphic :{	0b1111110000111100,
				0b1100000000000011,
				0b1111000000001100,
				0b1100000000110000,
				0b1111110000111111},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_1,
	next :		&edit_env_3,
	function :	Enter_Env2
};

struct menuNode edit_env_3 = {
	graphic :{	0b1111110000111100,
				0b1100000000000011,
				0b1111000000111100,
				0b1100000000000011,
				0b1111110000111100},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_2,
	next :		&edit_env_back,
	function :	Enter_Env3
};

// lvl3
struct menuNode edit_env_atk_n = {
	graphic :{	0b0000000000001100,
				0b0000000000110011,
				0b0000000011000000,
				0b0000001100000000,
				0b0000110000000000},
	kid :		&edit_env_atk_max_n,
	previous :	&edit_env_back_n,
	next :		&edit_env_dec_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_atk_max_n = {
	graphic :{	0b1100110011001100,
				0b1111110011001100,
				0b1100110000110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_atk_max_n,
	previous :	&edit_env_atk_back_n,
	next :		&edit_env_atk_min_n,
	function :	Set_Env_Atk_Max
};

struct menuNode edit_env_atk_min_n = {
	graphic :{	0b1100110000000000,
				0b1111110000000000,
				0b1100110011110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_atk_min_n,
	previous :	&edit_env_atk_max_n,
	next :		&edit_env_atk_bind_n,
	function :	Set_Env_Atk_Min
};

struct menuNode edit_env_atk_bind_n = {
	graphic :{	0b1111000011000000,
				0b1100110000000000,
				0b1111000011111100,
				0b1100110011110011,
				0b1111000011110011},
	kid :		&edit_env_atk_bind_n,
	previous :	&edit_env_atk_min_n,
	next :		&edit_env_atk_back_n,
	function :	Set_Env_Atk_Bind
};

struct menuNode edit_env_atk_back_n = {
	graphic :{	0b0000000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b1100000011000000,
				0b0000000011111111},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_atk_bind_n,
	next :		&edit_env_atk_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_dec_n = {
	graphic :{	0b0011000000000000,
				0b1100110000000000,
				0b1100001100000000,
				0b0000000011000000,
				0b0000000000111111},
	kid :		&edit_env_dec_max_n,
	previous :	&edit_env_atk_n,
	next :		&edit_env_sus_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_dec_max_n = {
	graphic :{	0b1100110011001100,
				0b1111110011001100,
				0b1100110000110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_dec_max_n,
	previous :	&edit_env_dec_back_n,
	next :		&edit_env_dec_min_n,
	function :	Set_Env_Dec_Max
};

struct menuNode edit_env_dec_min_n = {
	graphic :{	0b1100110000000000,
				0b1111110000000000,
				0b1100110011110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_dec_min_n,
	previous :	&edit_env_dec_max_n,
	next :		&edit_env_dec_bind_n,
	function :	Set_Env_Dec_Min
};

struct menuNode edit_env_dec_bind_n = {
	graphic :{	0b1111000011000000,
				0b1100110000000000,
				0b1111000011111100,
				0b1100110011110011,
				0b1111000011110011},
	kid :		&edit_env_dec_bind_n,
	previous :	&edit_env_dec_min_n,
	next :		&edit_env_dec_back_n,
	function :	Set_Env_Dec_Bind
};

struct menuNode edit_env_dec_back_n = {
	graphic :{	0b0000000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b1100000011000000,
				0b0000000011111111},
	kid :		&edit_env_dec_n,
	previous :	&edit_env_dec_bind_n,
	next :		&edit_env_dec_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_sus_n = {
	graphic :{	0b1100000000000000,
				0b0011000000000000,
				0b0000111111110000,
				0b0000000000001100,
				0b0000000000000011},
	kid :		&edit_env_sus_max_n,
	previous :	&edit_env_dec_n,
	next :		&edit_env_rel_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_sus_max_n = {
	graphic :{	0b1100110011001100,
				0b1111110011001100,
				0b1100110000110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_sus_max_n,
	previous :	&edit_env_sus_back_n,
	next :		&edit_env_sus_min_n,
	function :	Set_Env_Sus_Max
};

struct menuNode edit_env_sus_min_n = {
	graphic :{	0b1100110000000000,
				0b1111110000000000,
				0b1100110011110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_sus_min_n,
	previous :	&edit_env_sus_max_n,
	next :		&edit_env_sus_bind_n,
	function :	Set_Env_Sus_Min
};

struct menuNode edit_env_sus_bind_n = {
	graphic :{	0b1111000011000000,
				0b1100110000000000,
				0b1111000011111100,
				0b1100110011110011,
				0b1111000011110011},
	kid :		&edit_env_sus_bind_n,
	previous :	&edit_env_sus_min_n,
	next :		&edit_env_sus_back_n,
	function :	Set_Env_Sus_Bind
};

struct menuNode edit_env_sus_back_n = {
	graphic :{	0b0000000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b1100000011000000,
				0b0000000011111111},
	kid :		&edit_env_sus_n,
	previous :	&edit_env_sus_bind_n,
	next :		&edit_env_sus_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_rel_n = {
	graphic :{	0b0000000000000000,
				0b1111110000000000,
				0b0000001100000000,
				0b0000000011000000,
				0b0000000000110000},
	kid :		&edit_env_rel_max_n,
	previous :	&edit_env_sus_n,
	next :		&edit_env_back_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_rel_max_n = {
	graphic :{	0b1100110011001100,
				0b1111110011001100,
				0b1100110000110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_rel_max_n,
	previous :	&edit_env_rel_back_n,
	next :		&edit_env_rel_min_n,
	function :	Set_Env_Rel_Max
};

struct menuNode edit_env_rel_min_n = {
	graphic :{	0b1100110000000000,
				0b1111110000000000,
				0b1100110011110000,
				0b1100110011001100,
				0b1100110011001100},
	kid :		&edit_env_rel_min_n,
	previous :	&edit_env_rel_max_n,
	next :		&edit_env_rel_bind_n,
	function :	Set_Env_Rel_Min
};

struct menuNode edit_env_rel_bind_n = {
	graphic :{	0b1111000011000000,
				0b1100110000000000,
				0b1111000011111100,
				0b1100110011110011,
				0b1111000011110011},
	kid :		&edit_env_rel_bind_n,
	previous :	&edit_env_rel_min_n,
	next :		&edit_env_rel_back_n,
	function :	Set_Env_Rel_Bind
};

struct menuNode edit_env_rel_back_n = {
	graphic :{	0b0000000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b1100000011000000,
				0b0000000011111111},
	kid :		&edit_env_rel_n,
	previous :	&edit_env_rel_bind_n,
	next :		&edit_env_rel_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_back_n = {
	graphic :{	0b0000000011000000,
				0b0000001111110000,
				0b1100110011001100,
				0b0000000011000000,
				0b0000000011111111},
	kid :		&edit_env_back,
	previous :	&edit_env_rel_n,
	next :		&edit_env_atk_n,
	function :	Exit_Env
};

// lvl2
struct menuNode edit_env_back = {
	graphic :{	0b0000000011000000,
				0b1100001111110000,
				0b0000110011001100,
				0b0000000011000000,
				0b0000000011111111},
	kid :		&edit_envelope,
	previous :	&edit_env_3,
	next :		&edit_env_0,
	function :	Enter_Kid
};

// lvl1
struct menuNode edit_back_n = {
	graphic :{	0b1100000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b0000000011000000,
				0b0000000011111111},
	kid :		&edit_n,
	previous :	&edit_envelope,
	next :		&edit_bend_n,
	function :	Enter_Kid
};

// lvl0
struct menuNode save_n = {
	graphic :{	0b0011111100000011,
				0b1100001100000011,
				0b0011000011001100,
				0b0000110011001100,
				0b1111000000110000},
	kid :		&save_bind_pc_n,
	previous :	&edit_n,
	next :		&load_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode save_bind_pc_n = {
	graphic :{	0b1111000000111100,
				0b1100110011000000,
				0b1111000011000000,
				0b1100000011000000,
				0b1100000000111100},
	kid :		&save_slot_n,
	previous :	&save_back_n,
	next :		&save_slot_n,
	function :	Set_Save_PC
};

struct menuNode save_slot_n = {
	graphic :{	0b0000000011111111,
				0b0000000011111111,
				0b0000000000000011,
				0b0000000011111111,
				0b0000000011111111},
	kid :		&save_back_n,
	previous :	&save_bind_pc_n,
	next :		&save_back_n,
	function :	Set_Conf_Slot
};

struct menuNode save_back_n = {
	graphic :{	0b1100000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b0000000011000000,
				0b0000000011111111},
	kid :		&save_back_abort_n,
	previous :	&save_slot_n,
	next :		&save_bind_pc_n,
	function :	Enter_Kid
};

// lvl2
struct menuNode save_back_accept_n = {
	graphic :{	0b0000000000000011,
				0b0000000000001100,
				0b0011000000110000,
				0b0000110011000000,
				0b0000001100000000},
	kid :		&save_n,
	previous :	&save_back_abort_n,
	next :		&save_back_abort_n,
	function :	Save_Config
};

struct menuNode save_back_abort_n = {
	graphic :{	0b0000110000001100,
				0b0000001100110000,
				0b0000000011000000,
				0b0000001100110000,
				0b0000110000001100},
	kid :		&save_n,
	previous :	&save_back_accept_n,
	next :		&save_back_accept_n,
	function :	Enter_Kid
};

// lvl0
struct menuNode load_n = {
	graphic :{	0b1100000011110000,
				0b1100000011001100,
				0b1100000011001100,
				0b1100000011001100,
				0b1111110011110000},
	kid :		&load_slot_n,
	previous :	&save_n,
	next :		&edit_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode load_slot_n = {
	graphic :{	0b0000000011111111,
				0b0000000011111111,
				0b0000000000000011,
				0b0000000011111111,
				0b0000000011111111},
	kid :		&load_back_n,
	previous :	&load_back_n,
	next :		&load_back_n,
	function :	Set_Conf_Slot
};

struct menuNode load_back_n = {
	graphic :{	0b1100000011000000,
				0b0000001111110000,
				0b0000110011001100,
				0b0000000011000000,
				0b0000000011111111},
	kid :		&load_n,
	previous :	&load_slot_n,
	next :		&load_slot_n,
	function :	Load_Config
};



