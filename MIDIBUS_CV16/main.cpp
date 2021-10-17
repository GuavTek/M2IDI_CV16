/*
 * MIDIBUS_CV16.cpp
 *
 * Created: 04/07/2021 13:26:36
 * Author : GuavTek
 */ 


#include "sam.h"
#include "asf.h"
#include "LEDMatrix.h"
#include "menu.h"
#include "PWM.h"
#include "GenericOutput.h"
#include "MIDI_Driver.h"
#include "MCP2517.h"
#include "MIDI_Config.h"

void RTC_Init();

MCP2517_C CAN(SERCOM4);

MIDI_C MIDI(2);

void CAN_Receive(CAN_Rx_msg_t* msgIn);

int main(void)
{
	system_init();
	RTC_Init();
	LM_Init();
	PWM_Init();
	Menu_Init();
	GO_Init();
	CAN.Init(CAN_CONF, SPI_CONF);
	CAN.Set_Rx_Callback(CAN_Receive);
	MIDI.Set_handler(GO_MIDI_Voice);
	MIDI.Set_handler(GO_MIDI_Realtime);
	
	NVIC_EnableIRQ(SERCOM4_IRQn);
	system_interrupt_enable_global();
	
    /* Replace with your application code */
    while (1) 
    {
		static uint32_t periodic_timer = 0;
		if (periodic_timer < RTC->MODE0.COUNT.reg)	{
			periodic_timer = RTC->MODE0.COUNT.reg;
			GO_Service();
			Menu_Service();
			LM_Service();
			PWM_Service();
		}
		static uint32_t smiley_timer = 0;
		if (smiley_timer < RTC->MODE0.COUNT.reg) {
			smiley_timer = RTC->MODE0.COUNT.reg + 1024;
			LM_WriteRow(0, 0b00100100);
			LM_WriteRow(1, 0b00100100);
			LM_WriteRow(2, 0b00000000);
			LM_WriteRow(3, 0b01000010);
			LM_WriteRow(4, 0b00111100);
		}
		CAN.State_Machine();
    }
}

void RTC_Init(){
	// Enable clock
	PM->APBAMASK.bit.RTC_ = 1;
	
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_RTC;
	
	RTC->MODE0.READREQ.bit.RCONT = 1;
	
	RTC->MODE0.COUNT.reg = 0;
	
	RTC->MODE0.CTRL.bit.MODE = RTC_MODE0_CTRL_MODE_COUNT32_Val;
	RTC->MODE0.CTRL.bit.PRESCALER = RTC_MODE0_CTRL_PRESCALER_DIV1_Val;
	
	RTC->MODE0.CTRL.bit.ENABLE = 1;
}

void CAN_Receive(CAN_Rx_msg_t* msgIn){
	uint8_t length = CAN.Get_Data_Length(msgIn->dataLengthCode);
	MIDI.Decode(msgIn->payload, length);
	LM_WriteRow(0, 0b00100100);
	LM_WriteRow(1, 0b00000000);
	LM_WriteRow(2, 0b00111100);
	LM_WriteRow(3, 0b01000010);
	LM_WriteRow(4, 0b00111100);
}

// Sercom4 interrupt leads to sercom2 handler
// For some reason
void SERCOM2_Handler(){
	CAN.Handler();
}
