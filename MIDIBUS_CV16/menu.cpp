/*
 * menu.cpp
 *
 * Created: 19/07/2021 17:01:37
 *  Author: GuavTek
 */ 

#include "samd21.h"
#include "LEDMatrix.h"
#include "GenericOutput.h"
#include "menu.h"
#include "system_interrupt.h"

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

const void Enter_Kid()			{ currentNode = currentNode->kid; }
const void Enter_Env0()			{ chanSel = 0; Enter_Kid(); }
const void Enter_Env1()			{ chanSel = 1; Enter_Kid(); }
const void Enter_Env2()			{ chanSel = 2; Enter_Kid(); }
const void Enter_Env3()			{ chanSel = 3; Enter_Kid(); }
const void Exit_Env()			{ chanSel = 0; Enter_Kid(); }
const void Edit_Bend()			{ menuStatus = Edit_int; var_edit = &bendRange; max_edit = 8; }
const void Select_Pressure()	{ outMatrix[chanSel & 0b11][chanSel >> 2].type = GOType_t::Pressure; needScan = true; Enter_Kid(); }
const void Select_CV()			{ outMatrix[chanSel & 0b11][chanSel >> 2].type = GOType_t::DC; needScan = true; Enter_Kid(); }
const void Select_Gate()		{ outMatrix[chanSel & 0b11][chanSel >> 2].type = GOType_t::Gate; needScan = true; Enter_Kid(); }
const void Select_Envelope()	{ menuStatus = Edit_int; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].env_num; max_edit = 4; outMatrix[chanSel & 0b11][chanSel >> 2].type = GOType_t::Envelope; }
const void Select_Velocity()	{ outMatrix[chanSel & 0b11][chanSel >> 2].type = GOType_t::Velocity; needScan = true; Enter_Kid(); }
const void Select_Clk()			{ menuStatus = Edit_int; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].freq_current; outMatrix[chanSel & 0b11][chanSel >> 2].freq_current=12; max_edit = 128; outMatrix[chanSel & 0b11][chanSel >> 2].type = GOType_t::CLK; }
const void Select_LFO()			{ outMatrix[chanSel & 0b11][chanSel >> 2].type = GOType_t::LFO; needScan = true; Enter_Kid(); }
const void Select_LFO_Shape()	{ menuStatus = SetLFO; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].shape; }
const void Select_LFO_Freq_Max(){ menuStatus = Edit_32bit; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].freq_max; var_monitor = &outMatrix[chanSel & 0b11][chanSel >> 2].freq_current; }
const void Select_LFO_Freq_Min(){ menuStatus = Edit_32bit; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].freq_min; var_monitor = &outMatrix[chanSel & 0b11][chanSel >> 2].freq_current; }
const void Set_GO_MIDI()		{ menuStatus = Wait_MIDI; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].gen_source; midiTypeMask = 0b111; }
const void Set_Max_Range()		{ menuStatus = Edit_16bit; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].max_range; var_monitor = &outMatrix[chanSel & 0b11][chanSel >> 2].currentOut; }
const void Set_Min_Range()		{ menuStatus = Edit_16bit; var_edit = &outMatrix[chanSel & 0b11][chanSel >> 2].min_range; var_monitor = &outMatrix[chanSel & 0b11][chanSel >> 2].currentOut; }
const void Set_Conf_Slot()		{ menuStatus = Edit_int; var_edit = &confSlot; max_edit = MAX_SLOTS; }
const void Set_Save_PC()		{ menuStatus = Wait_MIDI; var_edit = &confPC; midiTypeMask = 0b100; }
const void Save_Config()		{ needSave = true; Enter_Kid(); }
const void Load_Config()		{ needLoad = true; Enter_Kid(); }
const void Set_Group()			{ menuStatus = Edit_int; var_edit = &midi_group; max_edit = 16; }
const void Set_Env_Atk_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].att_max; var_monitor = &envelopes[chanSel].att_current; }
const void Set_Env_Atk_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].att_min; var_monitor = &envelopes[chanSel].att_current; }
const void Set_Env_Atk_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].att_source; midiTypeMask = 0b001; }
const void Set_Env_Dec_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].dec_max; var_monitor = &envelopes[chanSel].dec_current; }
const void Set_Env_Dec_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].dec_min; var_monitor = &envelopes[chanSel].dec_current; }
const void Set_Env_Dec_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].dec_source; midiTypeMask = 0b001; }
const void Set_Env_Sus_Max()	{ menuStatus = Edit_16bit; var_edit = &envelopes[chanSel].sus_max; var_monitor = &envelopes[chanSel].sus_current; }
const void Set_Env_Sus_Min()	{ menuStatus = Edit_16bit; var_edit = &envelopes[chanSel].sus_min; var_monitor = &envelopes[chanSel].sus_current; }
const void Set_Env_Sus_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].sus_source; midiTypeMask = 0b001; }
const void Set_Env_Rel_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].rel_max; var_monitor = &envelopes[chanSel].rel_current; }
const void Set_Env_Rel_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].rel_min; var_monitor = &envelopes[chanSel].rel_current; }
const void Set_Env_Rel_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].rel_source; midiTypeMask = 0b001; }

// Used to bind MIDI sources in configuration
uint8_t Menu_MIDI(MIDI2_voice_t* msg){
	if (menuStatus == menu_status_t::Wait_MIDI){
		ctrlSource_t* tempSource = (ctrlSource_t*) var_edit;
		
		switch(msg->status){
			case MIDI2_VOICE_E::ProgChange:
				if (!(midiTypeMask & 0b100)){
					return 0;
				}
				// TODO: Set global bank?
				tempSource->sourceType = ctrlType_t::PC;
				tempSource->sourceNum = msg->program;
				break;
			case MIDI2_VOICE_E::NoteOn:
			case MIDI2_VOICE_E::NoteOff:
				if (!(midiTypeMask & 0b010)){
					return 0;
				}
				tempSource->sourceType = ctrlType_t::Key;
				tempSource->sourceNum = msg->note;
				break;
			case MIDI2_VOICE_E::CControl:
				if (!(midiTypeMask & 0b001)){
					return 0;
				}
				tempSource->sourceType = ctrlType_t::CC;
				tempSource->sourceNum = msg->controller;
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

void Menu_Init(){
	currentNode = &edit_n;
	menuStatus = Navigate;
	
	// Enable EIC clock
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | GCLK_CLKCTRL_ID_EIC;
	
	// Initialize buttons PA27(exint 15), PB22 (exint 6), PB23 (exint 7)
	PORT->Group[0].PINCFG[27].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_PMUXEN;
	PORT->Group[0].PMUX[13].bit.PMUXO = PORT_PMUX_PMUXO_A_Val;
	PORT->Group[0].OUTSET.reg = 1 << 27;
	PORT->Group[1].PINCFG[22].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_PMUXEN;
	PORT->Group[1].PMUX[11].bit.PMUXE = PORT_PMUX_PMUXE_A_Val;
	PORT->Group[1].PINCFG[23].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_PMUXEN;
	PORT->Group[1].PMUX[11].bit.PMUXO = PORT_PMUX_PMUXO_A_Val;
	PORT->Group[1].OUTSET.reg = (1 << 22) | (1 << 23);
	EIC->INTENSET.reg = EIC_INTENSET_EXTINT6 | EIC_INTENSET_EXTINT7 | EIC_INTENSET_EXTINT15;
	EIC->CONFIG[0].reg |= EIC_CONFIG_FILTEN6 | EIC_CONFIG_SENSE6_FALL | EIC_CONFIG_FILTEN7 | EIC_CONFIG_SENSE7_FALL;
	EIC->CONFIG[1].reg |= EIC_CONFIG_FILTEN7 | EIC_CONFIG_SENSE7_FALL;
	EIC->CTRL.bit.ENABLE = 1;
	
	NVIC_EnableIRQ(EIC_IRQn);
}

uint8_t Menu_Service(){
	bool screenChange = false;
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
				tempSrc->sourceType = ctrlType_t::None;
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
			Menu_Init();
			break;
	}
	
	if (screenChange) {
		if (menuStatus == menu_status_t::Navigate){
			// Channel select screen override
			if (currentNode == &edit_select_n){
				LM_WriteRow(0, 0xff);
				for (uint8_t i = 0; i < 4; i++){
					LM_WriteRow(i+1, 0b11000011 | (((chanSel % 4) == i) << (5 - chanSel/4)));
				}
			} else {
				for (uint8_t i = 0; i < 5; i++)	{
					LM_WriteRow(i, currentNode->graphic[i]);
				}
			}
		} else if (menuStatus == menu_status_t::Wait_MIDI){
			LM_WriteRow(0,0xff);
			LM_WriteRow(1,0);
			LM_WriteRow(2,0);
			LM_WriteRow(4,0);
			uint8_t timer = (RTC->MODE0.COUNT.reg >> 14) & 0b111;
			LM_WriteRow(3, 0b01010101 & ~(1 << timer));
		} else if (menuStatus == menu_status_t::SetLFO){
			WavShape_t* tempPoint = (WavShape_t*) var_edit;
			WavShape_t tempShape = *tempPoint;
			switch(tempShape){
				case WavShape_t::Square:
					LM_WriteRow(0, 0b01111001);
					LM_WriteRow(1, 0b01001001);
					LM_WriteRow(2, 0b01001001);
					LM_WriteRow(3, 0b01001001);
					LM_WriteRow(4, 0b11001111);
					break;
				case WavShape_t::Triangle:
					LM_WriteRow(0, 0b00001000);
					LM_WriteRow(1, 0b00010100);
					LM_WriteRow(2, 0b00100010);
					LM_WriteRow(3, 0b01000001);
					LM_WriteRow(4, 0b10000000);
					break;
				case WavShape_t::Sawtooth:
					LM_WriteRow(0, 0b10001000);
					LM_WriteRow(1, 0b11001100);
					LM_WriteRow(2, 0b10101010);
					LM_WriteRow(3, 0b10011001);
					LM_WriteRow(4, 0b10001000);
					break;
				case WavShape_t::Sine:
					LM_WriteRow(0, 0b00000001);
					LM_WriteRow(1, 0b00000110);
					LM_WriteRow(2, 0b00000100);
					LM_WriteRow(3, 0b10001100);
					LM_WriteRow(4, 0b01110000);
					break;
				case WavShape_t::SinSaw:
					LM_WriteRow(0, 0b11000110);
					LM_WriteRow(1, 0b10100101);
					LM_WriteRow(2, 0b10100100);
					LM_WriteRow(3, 0b10010100);
					LM_WriteRow(4, 0b10001100);
					break;
			}
		} else if (menuStatus == menu_status_t::Edit_int){
			uint8_t* tempPoint = (uint8_t*) var_edit;
			uint8_t tempNum = *tempPoint;
			uint8_t lo = tempNum & 0b111;
			uint8_t mid = (tempNum >> 3) & 0b111;
			uint8_t hi = tempNum >> 6;
			LM_WriteRow(0, 0xff);
			LM_WriteRow(1, 0);
			LM_WriteRow(2, 0xff >> (8-hi));
			LM_WriteRow(3, 0xff >> (8-mid));
			LM_WriteRow(4, 0xff >> (7-lo));
		} else {
			uint32_t tempNum;
			if (menuStatus == menu_status_t::Edit_8bit){
				uint8_t* tempPoint = (uint8_t*) var_edit;
				tempNum = *tempPoint | (*tempPoint << 8) | (*tempPoint << 16) | (*tempPoint << 24);
			} else if (menuStatus == menu_status_t::Edit_16bit){
				uint16_t* tempPoint = (uint16_t*) var_edit;
				tempNum = *tempPoint | (*tempPoint << 16);
			} else {
				uint32_t* tempPoint = (uint32_t*) var_edit;
				tempNum = *tempPoint;
			}
			LM_WriteRow(4, 0xff);
			LM_WriteRow(3, tempNum & 0xff);
			tempNum >>= 8;
			LM_WriteRow(2, tempNum & 0xff);
			tempNum >>= 8;
			LM_WriteRow(1, tempNum & 0xff);
			tempNum >>= 8;
			LM_WriteRow(0, tempNum & 0xff);
		}
		return 1;
	} else {
		return 0;
	}
}

menu_status_t Get_Menu_State(){
	return menuStatus;
}

/*
// Butt1 right, Butt2 up, Butt3 down
void EIC_Handler(){
	if (EIC->INTFLAG.reg & EIC_INTFLAG_EXTINT6)	{
		EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT6;
		buttRight = true;
	}
	
	if (EIC->INTFLAG.reg & EIC_INTFLAG_EXTINT7)	{
		EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT7;
		buttUp = true;
	}
	
	if (EIC->INTFLAG.reg & EIC_INTFLAG_EXTINT15)	{
		EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT15;
		buttDown = true;
	}
} */

// EIC Interrupt leads to WDT handler???!?
void WDT_Handler(){
	if (EIC->INTFLAG.reg & EIC_INTFLAG_EXTINT6)	{
		EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT6;
		buttRight = true;
	}
	
	if (EIC->INTFLAG.reg & EIC_INTFLAG_EXTINT7)	{
		EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT7;
		buttUp = true;
	}
	
	if (EIC->INTFLAG.reg & EIC_INTFLAG_EXTINT15)	{
		EIC->INTFLAG.reg = EIC_INTFLAG_EXTINT15;
		buttDown = true;
	}
}

// TODO: Improve node linking ( going down a level should lead to the first node at that level )

// lvl0
struct menuNode edit_n = {
	graphic :{	0b11101100, 
				0b10001010, 
				0b11001010, 
				0b10001010, 
				0b11101100},
	kid :		&edit_bend_n,
	previous :	&load_n,
	next :		&save_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode edit_bend_n = {
	graphic :{	0b11001001, 
				0b10101101, 
				0b11001101, 
				0b10101011, 
				0b11001001},
	kid :		&edit_n,
	previous :	&edit_back_n,
	next :		&edit_group,
	function :	Edit_Bend
};

struct menuNode edit_group = {
	graphic :{	0b11011011,
				0b11011011,
				0b00000000,
				0b00011011,
				0b00011011},
	kid :		&edit_group,
	previous :	&edit_bend_n,
	next :		&edit_select_n,
	function :	Set_Group
};

struct menuNode edit_select_n = {
	graphic :{	0b11111111, 
				0b11100011, 
				0b11000011, 
				0b11000011, 
				0b11000011},
	kid :		&edit_sel_type_n,
	previous :	&edit_group,
	next :		&edit_envelope,
	function :	Enter_Kid
};

// lvl2
struct menuNode edit_sel_max_range_n = {
	graphic :{	0b11000100,
				0b10101110,
				0b11010101,
				0b10100100,
				0b10100100},
	kid :		&edit_sel_max_range_n,
	previous :	&edit_sel_back_n,
	next :		&edit_sel_min_range_n,
	function :	Set_Max_Range
};

struct menuNode edit_sel_min_range_n = {
	graphic :{	0b11000100,
				0b10100100,
				0b11010101,
				0b10101110,
				0b10100100},
	kid :		&edit_sel_min_range_n,
	previous :	&edit_sel_max_range_n,
	next :		&edit_sel_bind_n,
	function :	Set_Min_Range
};

struct menuNode edit_sel_bind_n = {
	graphic :{	0b10001100,
				0b10001010,
				0b10001100,
				0b10001010,
				0b11101010},
	kid :		&edit_sel_bind_n,
	previous :	&edit_sel_min_range_n,
	next :		&edit_sel_type_n,
	function :	Set_GO_MIDI
};

struct menuNode edit_sel_type_n = {
	graphic :{	0b11101100,
				0b01001010,
				0b01001100,
				0b01001000,
				0b01001000},
	kid :		&edit_sel_type_CV_n,
	previous :	&edit_sel_bind_n,
	next :		&edit_sel_back_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_sel_type_CV_n = {
	graphic :{	0b01101010,
				0b10001010,
				0b10001010,
				0b10000100,
				0b01100100},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_back_n,
	next :		&edit_sel_type_gate_n,
	function :	Select_CV
};

struct menuNode edit_sel_type_gate_n = {
	graphic :{	0b01100111,
				0b10000010,
				0b10110010,
				0b10010010,
				0b01100010},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_CV_n,
	next :		&edit_sel_type_envelope_n,
	function :	Select_Gate
};

struct menuNode edit_sel_type_envelope_n = {
	graphic :{	0b00100000,
				0b01010000,
				0b01001100,
				0b10000010,
				0b10000001},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_gate_n,
	next :		&edit_sel_type_lfo_n,
	function :	Select_Envelope
};

struct menuNode edit_sel_type_lfo_n = {
	graphic :{	0b01110111,
				0b01010101,
				0b01010101,
				0b01010101,
				0b11011101},
	kid :		&edit_sel_type_lfo_shape_n,
	previous :	&edit_sel_type_envelope_n,
	next :		&edit_sel_type_clk_n,
	function :	Select_LFO
};

// lvl4
struct menuNode edit_sel_type_lfo_shape_n = {
	graphic :{	0b11000100,
				0b01001010,
				0b01010001,
				0b01100000,
				0b01000000},
	kid :		&edit_sel_type_lfo_fmax_n,
	previous :	&edit_sel_type_lfo_back_n,
	next :		&edit_sel_type_lfo_fmax_n,
	function :	Select_LFO_Shape
};

struct menuNode edit_sel_type_lfo_fmax_n = {
	graphic :{	0b11100100,
				0b10001110,
				0b11010101,
				0b10000100,
				0b10000100},
	kid :		&edit_sel_type_lfo_fmin_n,
	previous :	&edit_sel_type_lfo_shape_n,
	next :		&edit_sel_type_lfo_fmin_n,
	function :	Select_LFO_Freq_Max
};

struct menuNode edit_sel_type_lfo_fmin_n = {
	graphic :{	0b11100100,
				0b10000100,
				0b11010101,
				0b10001110,
				0b10000100},
	kid :		&edit_sel_type_lfo_back_n,
	previous :	&edit_sel_type_lfo_fmax_n,
	next :		&edit_sel_type_lfo_back_n,
	function :	Select_LFO_Freq_Min
};

struct menuNode edit_sel_type_lfo_back_n = {
	graphic :{	0b00001000,
				0b00011100,
				0b00101010,
				0b10001000,
				0b00001111},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_lfo_fmin_n,
	next :		&edit_sel_type_lfo_shape_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_sel_type_clk_n = {
	graphic :{	0b01110101,
				0b10010101,
				0b10010110,
				0b10010101,
				0b01111101},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_lfo_n,
	next :		&edit_sel_type_velocity_n,
	function :	Select_Clk
};

struct menuNode edit_sel_type_velocity_n = {
	graphic :{	0b10111010,
				0b10110010,
				0b10111010,
				0b01010010,
				0b01011011},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_clk_n,
	next :		&edit_sel_type_pressure_n,
	function :	Select_Velocity
};

struct menuNode edit_sel_type_pressure_n = {
	graphic :{	0b11011001,
				0b10110110,
				0b11011001,
				0b10010101,
				0b10010110},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_velocity_n,
	next :		&edit_sel_type_back_n,
	function :	Select_Pressure
};

struct menuNode edit_sel_type_back_n = {
	graphic :{	0b00001000,
				0b00011100,
				0b10101010,
				0b00001000,
				0b00001111},
	kid :		&edit_sel_type_n,
	previous :	&edit_sel_type_pressure_n,
	next :		&edit_sel_type_CV_n,
	function :	Enter_Kid
};

// lvl2
struct menuNode edit_sel_back_n = {
	graphic :{	0b00001000,
				0b10011100,
				0b00101010,
				0b00001000,
				0b00001111},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_n,
	next :		&edit_sel_max_range_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode edit_envelope = {
	graphic :{	0b00100000,
				0b01011000,
				0b01000100,
				0b10000010,
				0b10000001},
	kid :		&edit_env_0,
	previous :	&edit_select_n,
	next :		&edit_back_n,
	function :	Enter_Kid
};

// lvl2
struct menuNode edit_env_0 = {
	graphic :{	0b11100010,
				0b10000101,
				0b11000101,
				0b10000101,
				0b11100010},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_back,
	next :		&edit_env_1,
	function :	Enter_Env0
};

struct menuNode edit_env_1 = {
	graphic :{	0b11100010,
				0b10000010,
				0b11000010,
				0b10000010,
				0b11100010},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_0,
	next :		&edit_env_2,
	function :	Enter_Env1
};

struct menuNode edit_env_2 = {
	graphic :{	0b11100110,
				0b10000001,
				0b11000010,
				0b10000100,
				0b11100111},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_1,
	next :		&edit_env_3,
	function :	Enter_Env2
};

struct menuNode edit_env_3 = {
	graphic :{	0b11100110,
				0b10000001,
				0b11000110,
				0b10000001,
				0b11100110},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_2,
	next :		&edit_env_back,
	function :	Enter_Env3
};

// lvl3
struct menuNode edit_env_atk_n = {
	graphic :{	0b00000010,
				0b00000101,
				0b00001000,
				0b00010000,
				0b00100000},
	kid :		&edit_env_atk_max_n,
	previous :	&edit_env_back_n,
	next :		&edit_env_dec_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_atk_max_n = {
	graphic :{	0b10101010,
				0b11101010,
				0b10100100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_atk_max_n,
	previous :	&edit_env_atk_back_n,
	next :		&edit_env_atk_min_n,
	function :	Set_Env_Atk_Max
};

struct menuNode edit_env_atk_min_n = {
	graphic :{	0b10100000,
				0b11100000,
				0b10101100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_atk_min_n,
	previous :	&edit_env_atk_max_n,
	next :		&edit_env_atk_bind_n,
	function :	Set_Env_Atk_Min
};

struct menuNode edit_env_atk_bind_n = {
	graphic :{	0b11001000,
				0b10100000,
				0b11001110,
				0b10101101,
				0b11001101},
	kid :		&edit_env_atk_bind_n,
	previous :	&edit_env_atk_min_n,
	next :		&edit_env_atk_back_n,
	function :	Set_Env_Atk_Bind
};

struct menuNode edit_env_atk_back_n = {
	graphic :{	0b00001000,
				0b00011100,
				0b00101010,
				0b10001000,
				0b00001111},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_atk_bind_n,
	next :		&edit_env_atk_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_dec_n = {
	graphic :{	0b01000000,
				0b10100000,
				0b10010000,
				0b00001000,
				0b00000111},
	kid :		&edit_env_dec_max_n,
	previous :	&edit_env_atk_n,
	next :		&edit_env_sus_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_dec_max_n = {
	graphic :{	0b10101010,
				0b11101010,
				0b10100100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_dec_max_n,
	previous :	&edit_env_dec_back_n,
	next :		&edit_env_dec_min_n,
	function :	Set_Env_Dec_Max
};

struct menuNode edit_env_dec_min_n = {
	graphic :{	0b10100000,
				0b11100000,
				0b10101100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_dec_min_n,
	previous :	&edit_env_dec_max_n,
	next :		&edit_env_dec_bind_n,
	function :	Set_Env_Dec_Min
};

struct menuNode edit_env_dec_bind_n = {
	graphic :{	0b11001000,
				0b10100000,
				0b11001110,
				0b10101101,
				0b11001101},
	kid :		&edit_env_dec_bind_n,
	previous :	&edit_env_dec_min_n,
	next :		&edit_env_dec_back_n,
	function :	Set_Env_Dec_Bind
};

struct menuNode edit_env_dec_back_n = {
	graphic :{	0b00001000,
				0b00011100,
				0b00101010,
				0b10001000,
				0b00001111},
	kid :		&edit_env_dec_n,
	previous :	&edit_env_dec_bind_n,
	next :		&edit_env_dec_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_sus_n = {
	graphic :{	0b10000000,
				0b01000000,
				0b00111100,
				0b00000010,
				0b00000001},
	kid :		&edit_env_sus_max_n,
	previous :	&edit_env_dec_n,
	next :		&edit_env_rel_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_sus_max_n = {
	graphic :{	0b10101010,
				0b11101010,
				0b10100100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_sus_max_n,
	previous :	&edit_env_sus_back_n,
	next :		&edit_env_sus_min_n,
	function :	Set_Env_Sus_Max
};

struct menuNode edit_env_sus_min_n = {
	graphic :{	0b10100000,
				0b11100000,
				0b10101100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_sus_min_n,
	previous :	&edit_env_sus_max_n,
	next :		&edit_env_sus_bind_n,
	function :	Set_Env_Sus_Min
};

struct menuNode edit_env_sus_bind_n = {
	graphic :{	0b11001000,
				0b10100000,
				0b11001110,
				0b10101101,
				0b11001101},
	kid :		&edit_env_sus_bind_n,
	previous :	&edit_env_sus_min_n,
	next :		&edit_env_sus_back_n,
	function :	Set_Env_Sus_Bind
};

struct menuNode edit_env_sus_back_n = {
	graphic :{	0b00001000,
				0b00011100,
				0b00101010,
				0b10001000,
				0b00001111},
	kid :		&edit_env_sus_n,
	previous :	&edit_env_sus_bind_n,
	next :		&edit_env_sus_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_rel_n = {
	graphic :{	0b00000000,
				0b11100000,
				0b00010000,
				0b00001000,
				0b00000100},
	kid :		&edit_env_rel_max_n,
	previous :	&edit_env_sus_n,
	next :		&edit_env_back_n,
	function :	Enter_Kid
};

// lvl4
struct menuNode edit_env_rel_max_n = {
	graphic :{	0b10101010,
				0b11101010,
				0b10100100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_rel_max_n,
	previous :	&edit_env_rel_back_n,
	next :		&edit_env_rel_min_n,
	function :	Set_Env_Rel_Max
};

struct menuNode edit_env_rel_min_n = {
	graphic :{	0b10100000,
				0b11100000,
				0b10101100,
				0b10101010,
				0b10101010},
	kid :		&edit_env_rel_min_n,
	previous :	&edit_env_rel_max_n,
	next :		&edit_env_rel_bind_n,
	function :	Set_Env_Rel_Min
};

struct menuNode edit_env_rel_bind_n = {
	graphic :{	0b11001000,
				0b10100000,
				0b11001110,
				0b10101101,
				0b11001101},
	kid :		&edit_env_rel_bind_n,
	previous :	&edit_env_rel_min_n,
	next :		&edit_env_rel_back_n,
	function :	Set_Env_Rel_Bind
};

struct menuNode edit_env_rel_back_n = {
	graphic :{	0b00001000,
				0b00011100,
				0b00101010,
				0b10001000,
				0b00001111},
	kid :		&edit_env_rel_n,
	previous :	&edit_env_rel_bind_n,
	next :		&edit_env_rel_max_n,
	function :	Enter_Kid
};

// lvl3
struct menuNode edit_env_back_n = {
	graphic :{	0b00001000,
				0b00011100,
				0b10101010,
				0b00001000,
				0b00001111},
	kid :		&edit_env_back,
	previous :	&edit_env_rel_n,
	next :		&edit_env_atk_n,
	function :	Exit_Env
};

// lvl2
struct menuNode edit_env_back = {
	graphic :{	0b00001000,
				0b10011100,
				0b00101010,
				0b00001000,
				0b00001111},
	kid :		&edit_envelope,
	previous :	&edit_env_3,
	next :		&edit_env_0,
	function :	Enter_Kid
};

// lvl1
struct menuNode edit_back_n = {
	graphic :{	0b10001000,
				0b00011100,
				0b00101010,
				0b00001000,
				0b00001111},
	kid :		&edit_n,
	previous :	&edit_envelope,
	next :		&edit_bend_n,
	function :	Enter_Kid
};

// lvl0
struct menuNode save_n = {
	graphic :{	0b01110001,
				0b10010001,
				0b01001010,
				0b00101010,
				0b11000100},
	kid :		&save_bind_pc_n,
	previous :	&edit_n,
	next :		&load_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode save_bind_pc_n = {
	graphic :{	0b11000110,
				0b10101000,
				0b11001000,
				0b10001000,
				0b10000110},
	kid :		&save_slot_n,
	previous :	&save_back_n,
	next :		&save_slot_n,
	function :	Set_Save_PC
};

struct menuNode save_slot_n = {
	graphic :{	0b00001111,
				0b00001111,
				0b00000001,
				0b00001111,
				0b00001111},
	kid :		&save_back_n,
	previous :	&save_bind_pc_n,
	next :		&save_back_n,
	function :	Set_Conf_Slot
};

struct menuNode save_back_n = {
	graphic :{	0b10001000,
				0b00011100,
				0b00101010,
				0b00001000,
				0b00001111},
	kid :		&save_back_abort_n,
	previous :	&save_slot_n,
	next :		&save_bind_pc_n,
	function :	Enter_Kid
};

// lvl2
struct menuNode save_back_accept_n = {
	graphic :{	0b00000001,
				0b00000010,
				0b01000100,
				0b00101000,
				0b00010000},
	kid :		&save_n,
	previous :	&save_back_abort_n,
	next :		&save_back_abort_n,
	function :	Save_Config
};

struct menuNode save_back_abort_n = {
	graphic :{	0b00100010,
				0b00010100,
				0b00001000,
				0b00010100,
				0b00100010},
	kid :		&save_n,
	previous :	&save_back_accept_n,
	next :		&save_back_accept_n,
	function :	Enter_Kid
};

// lvl0
struct menuNode load_n = {
	graphic :{	0b10001100,
				0b10001010,
				0b10001010,
				0b10001010,
				0b11101100},
	kid :		&load_slot_n,
	previous :	&save_n,
	next :		&edit_n,
	function :	Enter_Kid
};

// lvl1
struct menuNode load_slot_n = {
	graphic :{	0b00001111,
				0b00001111,
				0b00000001,
				0b00001111,
				0b00001111},
	kid :		&load_back_n,
	previous :	&load_back_n,
	next :		&load_back_n,
	function :	Set_Conf_Slot
};

struct menuNode load_back_n = {
	graphic :{	0b10001000,
				0b00011100,
				0b00101010,
				0b00001000,
				0b00001111},
	kid :		&load_n,
	previous :	&load_slot_n,
	next :		&load_slot_n,
	function :	Load_Config
};



