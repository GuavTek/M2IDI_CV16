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
	}
}

