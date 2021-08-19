/*
 * PWM.cpp
 *
 * Created: 18/07/2021 16:37:49
 *  Author: GuavTek
 */ 

#include "sam.h"
#include "port.h"
#include "PWM.h"

uint16_t outLevel[4][4];

void PWM_Init(){
	// PA02, PA03, PA04, PA05 are PWM outputs
	PORT->Group[0].PMUX[1].reg = PORT_PMUX_PMUXE_F | PORT_PMUX_PMUXO_F;
	PORT->Group[0].PMUX[2].reg = PORT_PMUX_PMUXE_F | PORT_PMUX_PMUXO_F;
	PORT->Group[0].PINCFG[2].bit.PMUXEN = 1;
	PORT->Group[0].PINCFG[3].bit.PMUXEN = 1;
	PORT->Group[0].PINCFG[4].bit.PMUXEN = 1;
	PORT->Group[0].PINCFG[5].bit.PMUXEN = 1;
	
	// PA06 MUX inhibit
	PORT->Group[0].DIRSET.reg = 1 << 6;
	
	// PA00, PA01 are MUX select pins
	PORT->Group[0].DIRSET.reg = 3;
	
	// Enable TCC3 clock
	PM->APBCMASK.bit.TCC3_ = 1;
	
	// Select Generic clock (TCC3 ID was not defined)
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(0x25);
	
	// Set prescaler
	TCC3->CTRLA.bit.PRESCALER = TCC_CTRLA_PRESCALER_DIV1_Val;
	
	// Set PWM mode 
	TCC3->WAVEB.bit.WAVEGENB = TCC_WAVE_WAVEGEN_NPWM_Val;
	
	// Set PWM outputs
	TCC3->PATTB.reg = TCC_PATT_PGE0 | TCC_PATT_PGE1 | TCC_PATT_PGE2 | TCC_PATT_PGE3;
	
	// Set period
	TCC3->PERB.bit.PERB = 0x10000;
	
	// Enable timer
	TCC3->CTRLA.bit.ENABLE = 1;
}

void PWM_Set(uint8_t row, uint8_t col, uint16_t data){
	uint8_t x = 3 - col;
	uint8_t y;
	if (col & 1) {
		y = row ^ 1;
	} else {
		y = row;
	}
	outLevel[y][x] = data;
}

void PWM_Service(){
	// Inhibit MUX
	port_pin_set_output_level(6, 1);
		
	static uint8_t cycle;
	cycle++;
	cycle &= 0b11;
		
	switch(cycle){
		case 0:
			port_pin_set_output_level(0, 1);
			port_pin_set_output_level(1, 1);
			break;
		case 1:
			port_pin_set_output_level(0, 1);
			port_pin_set_output_level(1, 0);
			break;
		case 2:
			port_pin_set_output_level(0, 0); 
			port_pin_set_output_level(1, 0);
			break;
		case 3:
			port_pin_set_output_level(0, 0);
			port_pin_set_output_level(1, 1);
			break;
	}
		
	for (uint8_t i = 0; i < 4; i++)	{
		TCC3->CCB[i].bit.CCB = outLevel[cycle][i];
	}
		
	// Retrigger TCC
	TCC3->CTRLBSET.bit.CMD = TCC_CTRLBSET_CMD_RETRIGGER_Val;
		
	// Enable MUX
	port_pin_set_output_level(6, 0);
}