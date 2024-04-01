#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <hardware/spi.h>
#include <hardware/irq.h>
#include <hardware/timer.h>
#include "midi_config.h"
#include "SPI_RP2040.h"
#include "MCP2517.h"
// #include "AM_MIDI2"
#include "led_matrix.h"

void main1(void);

uint8_t dac_processed;
uint8_t dac_output;
bool dac_valid;

// Core0 main
int main(void){
	// Board init
	set_sys_clock_khz(120000, true);

    // Start core1
    multicore_reset_core1();
    sleep_ms(500);
    multicore_launch_core1(main1);
    while (true){
        sleep_ms(1);
    }
}

// Core1 main
void main1(void) {
    LM_Init();
    while (true){
        if(!dac_valid){
            static int32_t dac_count;   // Counts DAC iterations
            dac_valid = 1;
            // LED matrix, 30Hz* 5 rows * 4 levels times per second = 600Hz (=176400/294)
            if (dac_count >= 294){
                dac_count = 0;
                LM_Service();
            }

            // DAC multiplexing, 44,1kHz * 4 channels = 176400Hz
            dac_count++;
        }
    }
}
