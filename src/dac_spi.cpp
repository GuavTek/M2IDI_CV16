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
    pio_spi_cs_init(pio, pio_sm, offset, 24, clk_div, config.polarity, config.pin_cs, config.pin_tx);
	/*
	// Configure DMA channels
    dmaTx = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(dmaTx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, pio_sm, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_high_priority(&c, 1);
    dma_channel_set_config(dmaTx, &c, false);
	dma_channel_set_write_addr(dmaTx, &pio->txf[pio_sm], false);

	// Enable irq
	dmaNum = config.dma_irq_num;
	dma_irqn_set_channel_mask_enabled(config.dma_irq_num, (1 << dmaTx), true);
	*/
}

uint8_t __time_critical_func(DAC_SPI_C::Transfer)(char* buff, uint8_t length, com_state_e state){
	if (currentState == Idle){
        uint8_t l = 4;
        uint32_t buffer[4];
        for (uint8_t i = 0; i < 4; i++){
            for (uint8_t j = 0; j < 3; j++){
                uint8_t k = 3*i + j;
                if (k >= length){
                    l = i+1;
                    break;
                }
                buffer[i] &= ~(0xff << (8*(3-j)));
                buffer[i] |= buff[k] << (8*(3-j));
            }
        }
		currentState = Idle;
        for (uint8_t i = 0; i < l; i++){
            pio->txf[pio_sm] = buffer[i];
        }
        //currentState = state;
		//if ((state == Tx) || (state == RxTx)){
		//	dma_channel_set_trans_count(dmaTx, l, false);
	    //  dma_channel_set_read_addr(dmaTx, buffer, true);
		//}
		return 1;
	}
	return 0;
}

