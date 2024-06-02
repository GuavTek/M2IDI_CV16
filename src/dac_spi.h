/*
 * dac_spi.h
 *
 * Created: 26/05/2024
 *  Author: GuavTek
 */ 


#ifndef DAC_SPI_H_
#define DAC_SPI_H_
#include "communication_base.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "dac_spi.pio.h"
#include "hardware/dma.h"
#include "max5134.h"

struct dac_spi_config_t {
	uint8_t dma_irq_num;
	uint8_t polarity;
	uint32_t speed;
	uint8_t pin_tx;
	uint8_t pin_ck;
	uint8_t pin_cs;
};

class DAC_SPI_C : public communication_base_c
{
	public:
		void Init(const dac_spi_config_t config);
		uint8_t Transfer(char* buff, uint8_t length, com_state_e state);
		DAC_SPI_C(PIO const pio_inst) : communication_base_c(1), pio(pio_inst){pio_sm = pio_claim_unused_sm(pio_inst, 1);} ;
		~DAC_SPI_C(){};
	protected:
		PIO const pio;
        uint8_t pio_sm;
};

class fast_max5134_c : public max5134_c {
	public:
    void __time_critical_func(set)(uint16_t value[4], uint8_t write_thru){
    	for (uint8_t i = 0; i < 4; i++){
    	    dac_value[i] = value[i];
    	}
        write_to_dac(0);
    }
    uint8_t Set_SS(uint8_t enabled) {return 1;};
	fast_max5134_c(communication_base_c* const comInstance, uint8_t slaveNum) : max5134_c(comInstance, slaveNum) {
		// The DAC commands will not change
		dac_write_buff[0] = 0b00010000 | (1 << 0);
		dac_write_buff[3] = 0b00010000 | (1 << 1);
		dac_write_buff[6] = 0b00010000 | (1 << 2);
		dac_write_buff[9] = 0b00010000 | (1 << 3);
	}
    protected:
	char dac_write_buff[12];
    void __time_critical_func(write_to_dac)(uint8_t output){
		// Channel 0
        dac_write_buff[1] = (dac_value[0] >> 8) & 0xff;
        dac_write_buff[2] = dac_value[0] & 0xff;
        // Channel 1
        dac_write_buff[4] = (dac_value[1] >> 8) & 0xff;
        dac_write_buff[5] = dac_value[1] & 0xff;
		// Channel 2
        dac_write_buff[7] = (dac_value[2] >> 8) & 0xff;
        dac_write_buff[8] = dac_value[2] & 0xff;
		// Channel 3
        dac_write_buff[10] = (dac_value[3] >> 8) & 0xff;
        dac_write_buff[11] = dac_value[3] & 0xff;
        com->Transfer(dac_write_buff, 12, Tx);
    }
};

#endif /* DAC_SPI_H_ */