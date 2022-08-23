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

enum menu_status_t {
	Navigate,
	Learning,
	EditBend,
	SetEnvelope,
	SetLFO,
	SetRange,
	Saving,
	SetGroup,
	Edit_8bit,
	Edit_16bit,
	Edit_32bit,
	Edit_int,
	Wait_MIDI
} menuStatus;

menuNode* currentNode;
void* var_edit;
uint16_t max_edit;
uint8_t chanSel = 0;
volatile bool buttUp;
volatile bool buttDown;
volatile bool buttRight;

extern struct menuNode learn_n;
extern struct menuNode edit_n;
extern struct menuNode edit_bend_n;
extern struct menuNode edit_select_n;
extern struct menuNode edit_sel_back_n;
extern struct menuNode edit_sel_type_n;
extern struct menuNode edit_sel_type_pressure_n;
extern struct menuNode edit_sel_type_CV_n;
extern struct menuNode edit_sel_type_gate_n;
extern struct menuNode edit_sel_type_envelope_n;
extern struct menuNode edit_sel_type_velocity_n;
extern struct menuNode edit_sel_type_clk_n;
extern struct menuNode edit_sel_type_lfo_n;
extern struct menuNode edit_sel_type_lfo_tri_n;
extern struct menuNode edit_sel_type_lfo_saw_n;
extern struct menuNode edit_sel_type_lfo_sqr_n;
extern struct menuNode edit_sel_type_lfo_sin_n;
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
extern struct menuNode save_default_n;
extern struct menuNode save_pc_n;
extern struct menuNode save_back_n;
extern struct menuNode group_n;

const void Enter_Kid()			{ currentNode = currentNode->kid; }
const void Enter_Env0()			{ chanSel = 0; currentNode = currentNode->kid; }
const void Enter_Env1()			{ chanSel = 1; currentNode = currentNode->kid; }
const void Enter_Env2()			{ chanSel = 2; currentNode = currentNode->kid; }
const void Enter_Env3()			{ chanSel = 3; currentNode = currentNode->kid; }
const void Exit_Env()			{ chanSel = 0; currentNode = currentNode->kid; }
const void Enter_Selected()		{  }
const void Start_Learn()		{ menuStatus = Learning; }
const void Edit_Bend()			{ menuStatus = EditBend; }
const void Select_Pressure()	{  }
const void Select_CV()			{  }
const void Select_Gate()		{  }
const void Select_Envelope()	{ menuStatus = SetEnvelope; }
const void Select_Velocity()	{  }
const void Select_Clk()			{  }
const void Select_LFO_Tri()		{ menuStatus = SetLFO; }
const void Select_LFO_Saw()		{ menuStatus = SetLFO; }
const void Select_LFO_Sqr()		{ menuStatus = SetLFO; }
const void Select_LFO_Sin()		{ menuStatus = SetLFO; }
const void Set_Max_Range()		{ menuStatus = SetRange; }
const void Set_Min_Range()		{ menuStatus = SetRange; }
const void Save_Default()		{ menuStatus = Saving; }
const void Save_PC()			{ menuStatus = Saving; }
const void Set_Group()			{ menuStatus = Edit_int; var_edit = &midi_group; max_edit = 16; }
const void Set_Env_Atk_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].att_max; }
const void Set_Env_Atk_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].att_min; }
const void Set_Env_Atk_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].att_source; }
const void Set_Env_Dec_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].dec_max; }
const void Set_Env_Dec_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].dec_min; }
const void Set_Env_Dec_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].dec_source; }
const void Set_Env_Sus_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].sus_max; }
const void Set_Env_Sus_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].sus_min; }
const void Set_Env_Sus_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].sus_source; }
const void Set_Env_Rel_Max()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].rel_max; }
const void Set_Env_Rel_Min()	{ menuStatus = Edit_32bit; var_edit = &envelopes[chanSel].rel_min; }
const void Set_Env_Rel_Bind()	{ menuStatus = Wait_MIDI; var_edit = &envelopes[chanSel].rel_source; }

void Menu_Init(){
	currentNode = &learn_n;
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
		case Navigate:
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
		default:
			static uint32_t animateTime = 0;
			if (animateTime <= RTC->MODE0.COUNT.reg){
				static uint8_t animationStage;
				animateTime += 800;
			
				if (animationStage >= 31) {
					animationStage = 0;
				} else {
					animationStage++;
				}
			
				uint32_t temp = 7 << animationStage;
				for (uint8_t i = 0; i < 5; i++)	{
					LM_WriteRow(i, temp >> (12+i));
				}
			}
			break;
	}
	
	if (screenChange) {
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
		return 1;
	} else {
		return 0;
	}
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

struct menuNode learn_n = {
	graphic :{	0b10001100, 
				0b10001010, 
				0b10001100, 
				0b10001010, 
				0b11101010},
	kid :		&learn_n,
	previous :	&group_n,
	next :		&edit_n,
	function :	Start_Learn
};

struct menuNode edit_n = {
	graphic :{	0b11101100, 
				0b10001010, 
				0b11001010, 
				0b10001010, 
				0b11101100},
	kid :		&edit_bend_n,
	previous :	&learn_n,
	next :		&save_n,
	function :	Enter_Kid
};

struct menuNode edit_bend_n = {
	graphic :{	0b11001001, 
				0b10101101, 
				0b11001101, 
				0b10101011, 
				0b11001001},
	kid :		&edit_n,
	previous :	&edit_back_n,
	next :		&edit_envelope,
	function :	Edit_Bend
};

struct menuNode edit_select_n = {
	graphic :{	0b11111111, 
				0b11100011, 
				0b11000011, 
				0b11000011, 
				0b11000011},
	kid :		&edit_sel_type_n,
	previous :	&edit_envelope,
	next :		&edit_back_n,
	function :	Enter_Kid
};

struct menuNode edit_back_n = {
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_n,
	previous :	&edit_select_n,
	next :		&edit_bend_n,
	function :	Enter_Kid
};

struct menuNode edit_sel_back_n = {
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_select_n,
	previous :	&edit_sel_min_range_n,
	next :		&edit_sel_type_n,
	function :	Enter_Kid
};

struct menuNode edit_sel_type_n = {
	graphic :{	0b11101100,
				0b01001010,
				0b01001100,
				0b01001010,
				0b01001010},
	kid :		&edit_sel_type_CV_n,
	previous :	&edit_sel_back_n,
	next :		&edit_sel_max_range_n,
	function :	Enter_Kid
};

struct menuNode edit_sel_max_range_n = {
	graphic :{	0b11000100,
				0b10101110,
				0b11010101,
				0b10100100,
				0b10100100},
	kid :		&edit_sel_max_range_n,
	previous :	&edit_sel_type_n,
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
	next :		&edit_sel_back_n,
	function :	Set_Min_Range
};

struct menuNode edit_sel_type_pressure_n = {
	graphic :{	0b11011001,
				0b10110110,
				0b11011001,
				0b10010101,
				0b10010110},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_back_n,
	next :		&edit_sel_type_CV_n,
	function :	Select_Pressure
};

struct menuNode edit_sel_type_CV_n = {
	graphic :{	0b01101010,
				0b10001010,
				0b10001010,
				0b10000100,
				0b01100100},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_pressure_n,
	next :		&edit_sel_type_gate_n,
	function :	Select_CV
};

struct menuNode edit_sel_type_gate_n = {
	graphic :{	0b01100111,
				0b10000010,
				0b10110010,
				0b10010010,
				0b01100010},
	kid :		&edit_select_n,
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
	kid :		&edit_select_n,
	previous :	&edit_sel_type_gate_n,
	next :		&edit_sel_type_velocity_n,
	function :	Select_Envelope
};

struct menuNode edit_sel_type_velocity_n = {
	graphic :{	0b10111010,
				0b10110010,
				0b10111010,
				0b01010010,
				0b01011011},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_envelope_n,
	next :		&edit_sel_type_clk_n,
	function :	Select_Velocity
};

struct menuNode edit_sel_type_clk_n = {
	graphic :{	0b01110101,
				0b10010101,
				0b10010110,
				0b10010101,
				0b01111101},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_velocity_n,
	next :		&edit_sel_type_lfo_n,
	function :	Select_Clk
};

struct menuNode edit_sel_type_lfo_n = {
	graphic :{	0b10111010,
				0b10100101,
				0b10110101,
				0b10100101,
				0b11100010},
	kid :		&edit_sel_type_lfo_tri_n,
	previous :	&edit_sel_type_clk_n,
	next :		&edit_sel_type_back_n,
	function :	Enter_Kid
};

struct menuNode edit_sel_type_lfo_tri_n = {
	graphic :{	0b00001000,
				0b00010100,
				0b00100010,
				0b01000001,
				0b10000000},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_lfo_back_n,
	next :		&edit_sel_type_lfo_saw_n,
	function :	Select_LFO_Tri
};

struct menuNode edit_sel_type_lfo_saw_n = {
	graphic :{	0b10000100,
				0b11000110,
				0b10100101,
				0b10010100,
				0b10001100},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_lfo_tri_n,
	next :		&edit_sel_type_lfo_sqr_n,
	function :	Select_LFO_Saw
};

struct menuNode edit_sel_type_lfo_sqr_n = {
	graphic :{	0b11110011,
				0b10010010,
				0b10010010,
				0b10010010,
				0b10011110},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_lfo_saw_n,
	next :		&edit_sel_type_lfo_sin_n,
	function :	Select_LFO_Sqr
};

struct menuNode edit_sel_type_lfo_sin_n = {
	graphic :{	0b01110000,
				0b11011000,
				0b10001000,
				0b00001101,
				0b00000111},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_lfo_sqr_n,
	next :		&edit_sel_type_lfo_back_n,
	function :	Select_LFO_Sin
};

struct menuNode edit_sel_type_lfo_back_n = {
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_sel_type_lfo_n,
	previous :	&edit_sel_type_lfo_sin_n,
	next :		&edit_sel_type_lfo_tri_n,
	function :	Enter_Kid
};

struct menuNode edit_sel_type_back_n = {
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_select_n,
	previous :	&edit_sel_type_lfo_n,
	next :		&edit_sel_type_pressure_n,
	function :	Enter_Kid
};


struct menuNode edit_envelope = {
	graphic :{	0b00100000,
				0b01011000,
				0b01000100,
				0b10000010,
				0b10000001},
	kid :		&edit_env_0,
	previous :	&edit_bend_n,
	next :		&edit_select_n,
	function :	Enter_Kid
};

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

struct menuNode edit_env_back = {
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_3,
	next :		&edit_env_0,
	function :	Enter_Kid
};

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
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_env_atk_n,
	previous :	&edit_env_atk_bind_n,
	next :		&edit_env_atk_max_n,
	function :	Enter_Kid
};

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
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_env_dec_n,
	previous :	&edit_env_dec_bind_n,
	next :		&edit_env_dec_max_n,
	function :	Enter_Kid
};

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
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_env_sus_n,
	previous :	&edit_env_sus_bind_n,
	next :		&edit_env_sus_max_n,
	function :	Enter_Kid
};

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
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_env_rel_n,
	previous :	&edit_env_rel_bind_n,
	next :		&edit_env_rel_max_n,
	function :	Enter_Kid
};

struct menuNode edit_env_back_n = {
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&edit_env_back,
	previous :	&edit_env_rel_n,
	next :		&edit_env_atk_n,
	function :	Exit_Env
};


struct menuNode save_n = {
	graphic :{	0b01110001,
				0b10010001,
				0b01001010,
				0b00101010,
				0b11000100},
	kid :		&save_default_n,
	previous :	&edit_n,
	next :		&group_n,
	function :	Enter_Kid
};

struct menuNode save_default_n = {
	graphic :{	0b11001110,
				0b10101000,
				0b10101100,
				0b10101000,
				0b11001000},
	kid :		&save_n,
	previous :	&save_back_n,
	next :		&save_pc_n,
	function :	Save_Default
};

struct menuNode save_pc_n = {
	graphic :{	0b11000110,
				0b10101000,
				0b11001000,
				0b10001000,
				0b10000110},
	kid :		&save_n,
	previous :	&save_default_n,
	next :		&save_back_n,
	function :	Save_PC
};

struct menuNode save_back_n = {
	graphic :{	0b00010000,
				0b00111000,
				0b01010100,
				0b00010000,
				0b00011111},
	kid :		&save_n,
	previous :	&save_pc_n,
	next :		&save_default_n,
	function :	Enter_Kid
};

struct menuNode group_n = {
	graphic :{	0b11011011,
				0b11011011,
				0b00000000,
				0b00011011,
				0b00011011},
	kid :		&group_n,
	previous :	&save_n,
	next :		&learn_n,
	function :	Set_Group
};
