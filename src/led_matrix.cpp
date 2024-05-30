/*
 * led_matrix.cpp
 *
 * Created: 17/07/2021 18:32:08
 *  Author: GuavTek
 */ 

#include <stdio.h>
#include "pico/stdlib.h"
#include "board_m2idi_cv16.h"

uint8_t currentRow = 0b00000100;
uint8_t currentIntensity = 0;

uint16_t lmData[5] = {
	0xffff,
	0x17e4,
	0x83c2,
	0x0ff0,
	0xffff
};
const uint8_t LEDR[5] = { LEDR1, LEDR2, LEDR3, LEDR4, LEDR5 };
const uint8_t LEDC[8] = { LEDC1, LEDC2, LEDC3, LEDC4, LEDC5, LEDC6, LEDC7, LEDC8 };

void LM_Init(){
	for (uint8_t i = 0; i < 5; i++){
		gpio_init(LEDR[i]);
		gpio_set_dir(LEDR[i], GPIO_OUT);
		gpio_put(LEDR[i], 1);
	}
	for (uint8_t i = 0; i < 8; i++){
		gpio_init(LEDC[i]);
		gpio_set_dir(LEDC[i], GPIO_OUT);
		gpio_put(LEDC[i], 0);
	}
}

void LM_Service(){
	// Disable row
	gpio_put(LEDR[currentRow], 0);
	currentRow++;
	if (currentRow > 4){
		currentRow = 0;
		currentIntensity++;
		currentIntensity &= 0b11;
	}
	// Enable row
	gpio_put(LEDR[currentRow], 1);

	for (uint8_t i = 0; i < 8; i++){
		uint8_t pix_intensity;
		pix_intensity = (lmData[currentRow] >> 2*i) & 0b11;
		if (!pix_intensity) {
			// Disable column
			gpio_put(LEDC[i], 1);
		} else if (currentIntensity <= pix_intensity){
			// Enable column
			gpio_put(LEDC[i], 0);
		} else {
			// Disable column
			gpio_put(LEDC[i], 1);
		}
	}	
}

