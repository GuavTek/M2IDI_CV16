/*
 * LEDMatrix.cpp
 *
 * Created: 17/07/2021 18:32:08
 *  Author: GuavTek
 */ 

#include "sam.h"
#include "Port.h"
#include "LEDMatrix.h"

uint32_t LMTime = 0;
const uint32_t LMInterval = 1; // ms / 8

uint8_t currentRow = 0;
uint8_t currentCol = 0;

uint8_t lmData[5] = {255,36,24,36,255};
const uint8_t LEDR[5] = { 23, 22, 21, 20, 42 };
const uint8_t LEDC[8] = { 19, 18, 17, 43, 11, 10, 9, 8 };

void LM_Init(){
	PORT->Group[0].DIRSET.reg = 1 << LEDC1_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDC2_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDC3_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDC4_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDC6_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDC7_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDC8_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDR1_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDR2_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDR3_PIN;
	PORT->Group[0].DIRSET.reg = 1 << LEDR4_PIN;
	PORT->Group[1].DIRSET.reg = 1 << LEDC5_PIN;
	PORT->Group[1].DIRSET.reg = 1 << LEDR5_PIN;
	
	for (uint8_t i = 0; i < 8; i++)
	{
		port_pin_set_output_level(LEDC[i], 1);
	}
}

void LM_Service(){
	if (LMTime < RTC->MODE0.COUNT.reg) {
		LMTime = RTC->MODE0.COUNT.reg + LMInterval;
		
		// Disable column
		port_pin_set_output_level(LEDC[currentCol], 1);
		
		// Find the next pixel
		uint8_t tempCol = currentCol+1;
		bool found = false;
		bool newRow = false;
		for (uint8_t y = 0; y < 5; y++){
			uint8_t x = tempCol;
			uint8_t tempRow = currentRow + y;
			if (tempRow >= 5){
				tempRow -= 5;
			}
			for (; x < 8; x++){
				if (lmData[tempRow] & (1 << x)){
					currentRow = tempRow;
					currentCol = x;
					found = true;
					break;
				}
			}
			if (found){
				break;
			} else {
				tempCol = 0;
				// Disable row
				port_pin_set_output_level(LEDR[currentRow], 0);
				newRow = true;
			}
		}
		
		// Enable new row
		if (newRow){
			port_pin_set_output_level(LEDR[currentRow], 1);
		}
		
		// Enable column
		port_pin_set_output_level(LEDC[currentCol], 0);
	}
}

