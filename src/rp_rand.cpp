/*
 * rp_rand.cpp
 *
 * Created: 15/08/2024
 *  Author: GuavTek
 */

#include "hardware/clocks.h"
#include "hardware/structs/rosc.h"
#include "rp_rand.h"

volatile uint32_t dummy;
rp_rand_c rp_rand;

void rp_rand_c::init(uint8_t sample_num){
    // Set max ROSC frequency
    rosc_hw->ctrl = (ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB) | (ROSC_CTRL_FREQ_RANGE_VALUE_HIGH << ROSC_CTRL_FREQ_RANGE_LSB);
    rosc_hw->freqa = (ROSC_FREQA_PASSWD_VALUE_PASS << ROSC_FREQA_PASSWD_LSB) | 0xFFFF;
    rosc_hw->freqb = (ROSC_FREQB_PASSWD_VALUE_PASS << ROSC_FREQB_PASSWD_LSB) | 0xFFFF;
    
    // Configure DMA
    load_chan = dma_claim_unused_channel(true);
    sample_chan = dma_claim_unused_channel(true);
    int8_t temp_timer = dma_claim_unused_timer(true);
    
    // Configure dma timer
    uint32_t sample_freq_khz = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC) / 16;
    uint32_t div = clock_get_hz(clk_sys) / (1000 * sample_freq_khz);
    dma_timer_set_fraction(temp_timer, 1, div);

    // Configure channel which loads CRC32 value to the buffer
    dma_channel_config c = dma_channel_get_default_config(load_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_ring(&c, true, 5);   // 8x 32-bit values
    channel_config_set_chain_to(&c, sample_chan);
    dma_channel_set_config(load_chan, &c, false);
    dma_channel_set_trans_count(load_chan, 1, false);
    dma_channel_set_read_addr(load_chan, &dma_hw->sniff_data, false);
	dma_channel_set_write_addr(load_chan, buff, false);

    // Configure channel which samples the ROSC random bit
    c = dma_channel_get_default_config(sample_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, dma_get_timer_dreq(temp_timer));
    channel_config_set_sniff_enable(&c, true);
    channel_config_set_chain_to(&c, load_chan);
    dma_channel_set_config(sample_chan, &c, false);
    dma_channel_set_trans_count(sample_chan, sample_num, false);
    dma_channel_set_read_addr(sample_chan, &rosc_hw->randombit, false);
	dma_channel_set_write_addr(sample_chan, &dummy, false);
    
    // Configure dma sniffer for CRC32
    dma_sniffer_enable(sample_chan, DMA_SNIFF_CTRL_CALC_VALUE_CRC32, true);

    dma_channel_start(sample_chan);
}

uint32_t rp_rand_c::get(){
    index &= 0b111;
    return buff[index++];
}
