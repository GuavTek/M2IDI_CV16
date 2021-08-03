/*
 * GenericOutput.cpp
 *
 * Created: 19/07/2021 18:09:51
 *  Author: GuavTek
 */ 

#include "GenericOutput.h"
#include "PWM.h"

void GO_Init(){
	// Set default values
	PWM_Set(0,0, 0x0);
	PWM_Set(0,1, 0x0);
	PWM_Set(0,2, 0x0);
	PWM_Set(0,3, 0x0);
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
	
	// Load setup from NVM
	
}

void GO_Service(){
	static uint32_t oscTimer = 0;
	if (oscTimer <= RTC->MODE0.COUNT.reg){
		oscTimer += 16;
		static uint16_t tempOut1;
		static uint16_t tempOut2;
		static uint16_t tempOut3;
		static uint16_t tempOut4;
		static uint16_t tempOut5;
		tempOut1 += 90;
		tempOut2 += 50;
		tempOut3 += 20;
		tempOut4 += 1;
		tempOut5 -= 100;
		PWM_Set(2,2,tempOut1);
		PWM_Set(3,3,tempOut2);
		PWM_Set(2,1,tempOut3);
		PWM_Set(1,0,tempOut4);
		PWM_Set(3,0,tempOut5);
	}
}

