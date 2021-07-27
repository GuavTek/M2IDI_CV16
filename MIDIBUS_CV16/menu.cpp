/*
 * menu.cpp
 *
 * Created: 19/07/2021 17:01:37
 *  Author: GuavTek
 */ 

#include "samd21.h"
#include "LEDMatrix.h"

void Menu_Init(){
	// Connect menu nodes
}

void Menu_Service(){
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
}