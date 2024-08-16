/*
 * rp_rand.h
 *
 * Created: 15/08/2024
 *  Author: GuavTek
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"

class rp_rand_c {
public:
    void init(uint8_t sample_num);
    uint32_t get();
    void set_num_samples(uint8_t num){dma_channel_set_trans_count(sample_chan, num, false);}
protected:
    uint8_t sample_chan;
    uint8_t load_chan;
    uint8_t index;
    uint32_t __attribute__((aligned(8*4))) buff[8];
};

extern rp_rand_c rp_rand; 
