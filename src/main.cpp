#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <hardware/spi.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/timer.h>
#include "midi_config.h"
#include "SPI_RP2040.h"
#include "MCP2517.h"
#include "umpProcessor.h"
#include "utils.h"
#include "led_matrix.h"
#include "generic_output.h"
#include "menu.h"

void main1(void);
void dac_pwm_handler();
void CAN_Receive_Header(CAN_Rx_msg_t* data);
void CAN_Receive_Data(char* data, uint8_t length);

SPI_RP2040_C SPI_CAN = SPI_RP2040_C(spi0);
SPI_RP2040_C SPI = SPI_RP2040_C(spi1);
MCP2517_C CAN = MCP2517_C(&SPI_CAN);
//MIDI_C MIDI(2);

uint8_t dac_processed;
uint8_t dac_output;
bool dac_valid;
uint16_t dacLevel[4][4];

// Core0 main
int main(void){
	// Board init
	set_sys_clock_khz(120000, true);

    SPI_CAN.Init(SPI_CAN_CONF);
    CAN.Init(CAN_CONF);
    CAN.Set_Rx_Header_Callback(CAN_Receive_Header);
    CAN.Set_Rx_Data_Callback(CAN_Receive_Data);

    menu_init();

    // Start core1
    multicore_reset_core1();
    sleep_ms(500);
    multicore_launch_core1(main1);
    while (true){
        sleep_ms(1);
        static uint32_t smiley_timer = 0;
        if (menu_service())	{
			// Inactivity timeout
			smiley_timer = time_us_32() + 30000000;
		}
		if (smiley_timer < time_us_32()) {  // TODO: handle timer wrapping
			smiley_timer = time_us_32() + 1000000;

			LM_WriteRow(0, 0b00100100);
			LM_WriteRow(1, 0b00100100);
			LM_WriteRow(2, 0b00000000);
			LM_WriteRow(3, 0b01000010);
			LM_WriteRow(4, 0b00111100);

		}
    }
}

// Handle MIDI CAN data
void CAN_Receive_Header(CAN_Rx_msg_t* data){
	// Detect CAN id, and MIDI muid collisions
	// TODO
}

// Handle MIDI CAN data
void CAN_Receive_Data(char* data, uint8_t length){
	// Receive MIDI payload from CAN
	//MIDI_CAN.Decode(data, length);

    if (get_menu_state() == menu_status_t::Navigate){
		LM_WriteRow(0, 0b00100100);
		LM_WriteRow(1, 0b00000000);
		LM_WriteRow(2, 0b00111100);
		LM_WriteRow(3, 0b01000010);
		LM_WriteRow(4, 0b00111100);		// TODO: update to new ledmatrix format
	}
}

// Core1 main
void main1(void) {
    LM_Init();

    // Set up PWM for DAC multiplexing
    gpio_set_function(M2IDI_MUXINH_PIN, GPIO_FUNC_PWM);
    pwm_config pwm_conf = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&pwm_conf, PWM_DIV_FREE_RUNNING);
    pwm_config_set_clkdiv(&pwm_conf, 120000.0/(176.4 * 16));
    pwm_config_set_wrap(&pwm_conf, 15);
    pwm_set_chan_level(pwm_gpio_to_slice_num(M2IDI_MUXINH_PIN), PWM_CHAN_A, 1);
    pwm_clear_irq(pwm_gpio_to_slice_num(M2IDI_MUXINH_PIN));
    pwm_set_irq_enabled(pwm_gpio_to_slice_num(M2IDI_MUXINH_PIN), true);
    pwm_init(pwm_gpio_to_slice_num(M2IDI_MUXINH_PIN), &pwm_conf, true);
    
    gpio_init(M2IDI_MUXA_PIN);
    gpio_init(M2IDI_MUXB_PIN);
    gpio_set_dir(M2IDI_MUXA_PIN, GPIO_OUT);
    gpio_set_dir(M2IDI_MUXB_PIN, GPIO_OUT);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, dac_pwm_handler);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    while (true){
        uint8_t next_dac = dac_processed +1;
        next_dac &= 0b11;
        if (next_dac != dac_output){
            // Calculate a set of four values
            GO_Service(next_dac);
            dac_processed = next_dac;
        }
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
            // TODO: write next set of values to DAC
        }
    }
}


void dac_pwm_handler(){
    static uint8_t out_num;
    pwm_clear_irq(pwm_gpio_to_slice_num(M2IDI_MUXINH_PIN));
    dac_valid = 0;
    dac_output++;
    dac_output &= 0b11;

    // Switch mux pin
	switch(out_num){
		case 0:
            gpio_put(M2IDI_MUXA_PIN, 0);
            gpio_put(M2IDI_MUXB_PIN, 0);
			break;
		case 1:
            gpio_put(M2IDI_MUXA_PIN, 1);
            gpio_put(M2IDI_MUXB_PIN, 0);
			break;
		case 2:
            gpio_put(M2IDI_MUXA_PIN, 0);
            gpio_put(M2IDI_MUXB_PIN, 1);
			break;
		case 3:
            gpio_put(M2IDI_MUXA_PIN, 1);
            gpio_put(M2IDI_MUXB_PIN, 1);
			break;
	}

    out_num++;
    out_num &= 0b11;
}
