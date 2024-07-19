/*
 * DAC_SPI.cpp
 *
 * Created: 26/05/2024
 *  Author: GuavTek
 */ 

#include <stdio.h>
#include <stdlib.h>
#include "DAC_SPI.h"

void DAC_SPI_C::Init(const dac_spi_config_t config){
    // Calculate clock, pio needs 2 clock cycles per SPI bit
    float clk_div = (120.0 * 1000000 / config.speed) / 2;
    // Initialize pio
    uint32_t offset = pio_add_program(pio, &spi_tx_cs_program);
    pio_spi_cs_init(pio, pio_sm, offset, 24, clk_div, config.polarity, config.pin_ck, config.pin_tx);
	
	currentState = Idle;
}

uint8_t __time_critical_func(DAC_SPI_C::Transfer)(char* buff, uint8_t length, com_state_e state){
    for (uint8_t i = 0; i < length; i += 3){
        uint32_t buffer;
        buffer = buff[i] << 24;
        buffer |= buff[i+1] << 16;
        buffer |= buff[i+2] << 8;
        pio->txf[pio_sm] = buffer;
    }
	return 1;
}

