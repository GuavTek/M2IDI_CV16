#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <hardware/spi.h>
#include <hardware/irq.h>
#include <hardware/timer.h>
#include "MIDI_Config.h"
#include "SPI_RP2040.h"
#include "MCP2517.h"
// #include "AM_MIDI2"

void main1(void);

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

    while (true){
        static int32_t dac_count;   // Counts DAC iterations
        // LED matrix, updated 30*5*4 times per second = 600Hz (=176400/294)
        if (dac_count >= 294){
            sleep_ms(1);
        }
        
        // DAC multiplexing, 44,1kHz*4 = 176400Hz
        dac_count++;
        sleep_ms(1);
        // Put core to sleep if the next set of output values are valid
        
    }
}
