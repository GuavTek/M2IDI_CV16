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
		inline void Handler();
		uint8_t Transfer(char* buff, uint8_t length, com_state_e state);
		inline int8_t get_dma_tx() {return dmaTx;}
		DAC_SPI_C(PIO const pio_inst) : communication_base_c(1), pio(pio_inst){pio_sm = pio_claim_unused_sm(pio_inst, 1);} ;
		~DAC_SPI_C(){};
	protected:
		PIO const pio;
        uint8_t pio_sm;
		uint8_t dmaNum;
		int8_t dmaTx;
        uint32_t buffer[4];
};

// SPI interrupt handler
inline void DAC_SPI_C::Handler(){
	if (currentState == Idle){
		return;
	}
	if (dma_irqn_get_channel_status(dmaNum, dmaTx)){
		dma_irqn_acknowledge_channel(dmaNum, dmaTx);
		currentState = Idle;
		slaveCallbacks[0]->com_cb();
	}
}

class fast_max5134_c : public max5134_c {
	public:
    void set(uint16_t value[4], uint8_t write_thru){
        max5134_c::set(value, write_thru);
        write_to_dac(0);
    }
    uint8_t Set_SS(uint8_t enabled) {return 1;};
    protected:
    void write_to_dac(uint8_t output){
        char temp_buff[12];
        uint8_t bi = 0;
        for (uint8_t i = 0; i < 4; i++){
            if (need_update & (1 << i)) {
                temp_buff[bi] = 0b00010000 | (1 << i);
                temp_buff[bi++] |= ((need_update >> 4) << (5-i)) & 0b00100000; // Enable write-through?
                temp_buff[bi++] = (dac_value[i] >> 8) & 0xff;
                temp_buff[bi++] = dac_value[i] & 0xff;
            }
        }
        if (com->Get_Status() == com_state_e::Idle){
            com->Transfer(temp_buff, bi, Tx);
            need_update = 0;
        }
    }
    using max5134_c::max5134_c;
};

#endif /* DAC_SPI_H_ */