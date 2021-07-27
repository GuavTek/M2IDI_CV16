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

void RTC_Init();

int main(void)
{
	system_init();
	RTC_Init();
	LM_Init();
	PWM_Init();
	Menu_Init();
	
	PWM_Set(0,0, 0xffff);
	PWM_Set(1,1, 0xffff);
	PWM_Set(2,2, 0xffff);
	PWM_Set(3,3, 0xffff);
	PWM_Set(0,3, 0x4000);
	PWM_Set(1,2, 0x4000);
	PWM_Set(2,1, 0x4000);
	PWM_Set(3,0, 0x4000);
	
    /* Replace with your application code */
    while (1) 
    {
		Menu_Service();
		LM_Service();
		PWM_Service();
    }
}

void RTC_Init(){
	// Enable clock
	PM->APBAMASK.bit.RTC_ = 1;
	
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_RTC;
	
	RTC->MODE0.READREQ.bit.RCONT = 1;
	
	RTC->MODE0.CTRL.bit.MODE = RTC_MODE0_CTRL_MODE_COUNT32_Val;
	RTC->MODE0.CTRL.bit.PRESCALER = RTC_MODE0_CTRL_PRESCALER_DIV4_Val;
	
	RTC->MODE0.CTRL.bit.ENABLE = 1;
}
