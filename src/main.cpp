#include <stdio.h>
#include "pico/stdlib.h"
#include "board_m2idi_cv16.h"
#include "pico/multicore.h"
#include <hardware/clocks.h>
#include <hardware/spi.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/timer.h>
#include "midi_config.h"
#include "SPI_RP2040.h"
#include "dac_spi.h"
#include "MCP2517.h"
#include "max5134.h"
#include "eeprom_cat.h"
#include "umpProcessor.h"
#include "utils.h"
#include "led_matrix.h"
#include "generic_output.h"
#include "menu.h"
#include "memory.h"
#include <hardware/pio.h>
#include "ring_buffer.h"
#include "rp_rand.h"

void main1(void);
void dac_pwm_handler();
void CAN_Receive_Header(CAN_Rx_msg_t* data);
void CAN_Receive_Data(char* data, uint8_t length);
void dma0_irq_handler ();
void dma1_irq_handler ();

void midi_cvm_handler(struct umpCVM msg);
void midi_com_handler(struct umpGeneric msg);
void midi_stream_discovery(uint8_t majVer, uint8_t minVer, uint8_t filter);
void midi_data_handler(struct umpData msg);

SPI_RP2040_C SPI_CAN = SPI_RP2040_C(spi0,2);
MCP2517_C CAN = MCP2517_C(&SPI_CAN, 0);
eeprom_cat_c EEPROM = eeprom_cat_c(&SPI_CAN, 1);
DAC_SPI_C SPI_DAC = DAC_SPI_C(pio0);
fast_max5134_c DAC = fast_max5134_c(&SPI_DAC, 0);
umpProcessor MIDI;
ring_buffer_c<64, uint32_t> midi_buffer = ring_buffer_c<64, uint32_t>();

bool group_updated;
uint8_t current_group;
uint8_t dac_processed;
uint8_t dac_output;
bool dac_valid;
uint32_t smiley_timer = 0;

// Core0 main
int main(void){
	// Board init
	set_sys_clock_khz(120000, true);
	rp_rand.init(100);
    SPI_CAN.Init(SPI_CAN_CONF);
	irq_set_exclusive_handler(DMA_IRQ_1, dma1_irq_handler);
	irq_set_enabled(DMA_IRQ_1, true);

    CAN.Init(CAN_CONF);
    CAN.Set_Rx_Header_Callback(CAN_Receive_Header);
    CAN.Set_Rx_Data_Callback(CAN_Receive_Data);

	mem_handler = &EEPROM;
	mem_init();

    MIDI.setSystem(midi_com_handler);
	MIDI.setCVM(midi_cvm_handler);
	MIDI.setSysEx(midi_data_handler);
	MIDI.setMidiEndpoint(midi_stream_discovery);
	current_group = 1;
	group_updated = true;

    menu_init();

	gpio_init(M2IDI_CAN_INT_PIN);

    // Start core1
    multicore_reset_core1();
    sleep_ms(500);
    multicore_launch_core1(main1);
    while (true){
		if (group_updated){
			group_updated = false;
			CAN_Filter_t tempFilt = CAN_FLT0;
			tempFilt.ID = (current_group & 0xF) << 7;
			CAN.Reconfigure_Filter(&tempFilt, 0);
		}
        if (menu_service())	{
			// Inactivity timeout
			smiley_timer = time_us_32() + 30000000;
		}
		if (smiley_timer < time_us_32()) {
			smiley_timer = -1;

			LM_WriteRow(0, 0b0000110000110000);
			LM_WriteRow(1, 0b0000100000100000);
			LM_WriteRow(2, 0b0000000000000000);
			LM_WriteRow(3, 0b0010000000001000);
			LM_WriteRow(4, 0b0001111111110100);

		}
		if (midi_buffer.Count() > 0){
			MIDI.processUMP(midi_buffer.Read());
		}
		if (CAN.Ready()){
			if (!gpio_get(M2IDI_CAN_INT_PIN)){
				CAN.Check_Rx();
			}
		}
		mem_update();
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
	for (uint8_t i = 0; i < length; i += 4){
		uint32_t temp_data;
		temp_data = data[i] << 24;
		temp_data |= data[i+1] << 16;
		temp_data |= data[i+2] << 8;
		temp_data |= data[i+3];
		midi_buffer.Write(temp_data);
	}
    if (get_menu_state() == menu_status_t::Navigate){
		smiley_timer = time_us_32() + 500000;
		LM_WriteRow(0, 0b0000110000110000);
		LM_WriteRow(1, 0b0000100000100000);
		LM_WriteRow(2, 0b0000001111000000);
		LM_WriteRow(3, 0b0000110000110000);
		LM_WriteRow(4, 0b0000011111010000);
	}
}

void midi_cvm_handler(struct umpCVM msg){
	if (msg.umpGroup != current_group){
		return;
	}
	menu_midi(&msg);
	GO_MIDI_Voice(&msg);
	if ((msg.status == PROGRAM_CHANGE) && (!block_conf_pc)){
		uint32_t pc = msg.value;	// TODO: handle bank
		mem_read_config_pc(pc);
	}
}

void midi_com_handler(struct umpGeneric msg){
	if (msg.umpGroup != current_group){
		return;
	}
    GO_MIDI_Realtime(&msg);
}

void midi_stream_discovery(uint8_t majVer, uint8_t minVer, uint8_t filter){
	//Upon Recieving the filter it is important to return the information requested
	if(filter & 0x1){ //Endpoint Info Notification
		//std::array<uint32_t, 4> ump = UMPMessage::mtFMidiEndpointInfoNotify(1, true, true, false, false);
		//sendUMP(ump.data(),4);
	}

	if(filter & 0x2) {
		//std::array<uint32_t, 4> ump = UMPMessage::mtFMidiEndpointDeviceInfoNotify(
		//{MIDI_MFRID & 0xff, (MIDI_MFRID >> 8) & 0xff, (MIDI_MFRID >> 16) & 0xff},
		//{MIDI_FAMID & 0xff, (MIDI_FAMID >> 8) & 0xff},
		//{DEVICE_MODELID & 0xff, (DEVICE_MODELID >> 8) & 0xff},
		//{DEVICE_VERSIONID & 0xff, (DEVICE_VERSIONID >> 8) & 0xff, (DEVICE_VERSIONID >> 16) & 0xff, (DEVICE_VERSIONID >> 24) & 0xff});
		//sendUMP( ump.data(), 4);
	}

	if(filter & 0x4) {
		//uint8_t friendlyNameLength = sizeof(DEVICE_NAME);
		//for(uint8_t offset=0; offset<friendlyNameLength; offset+=14) {
		//	std::array<uint32_t, 4> ump = UMPMessage::mtFMidiEndpointTextNotify(MIDIENDPOINT_NAME_NOTIFICATION, offset, (uint8_t *) DEVICE_NAME,friendlyNameLength);
			//sendUMP(ump.data(),4);
		//}
	}

	if(filter & 0x8) {
		// TODO: read MCU unique ID
		//int8_t piiLength = sizeof(PRODUCT_INSTANCE_ID);
		//for(uint8_t offset=0; offset<piiLength; offset+=14) {
		//	std::array<uint32_t, 4> ump = UMPMessage::mtFMidiEndpointTextNotify(PRODUCT_INSTANCE_ID, offset, (uint8_t *) buff,piiLength);
		//	//sendUMP(ump.data(),4);
		//}
	}

	if(filter & 0x10){
		//std::array<uint32_t, 4> ump = UMPMessage::mtFNotifyProtocol(0x2,false,false);
		//sendUMP(ump.data(),4);
	}
}

void midi_data_handler(struct umpData msg){
	// TODO: implement Capability exchange features
}

// Core1 main
void main1(void) {
	const uint32_t core_clk = 120000000;
	const uint32_t dac_rate = 4*out_rate; // The rate of DAC outputs
	const float dac_period = 1.0 / dac_rate;
	const float dac_delay = 0.000008; //0.000005;	// DAC has 5µs settling time (7µs for 5v range)
	const uint16_t pwm_count = 64;
	const float pwm_period = dac_period / pwm_count;
	const float pwm_div = core_clk/float(dac_rate * pwm_count);	// Max divider value is ~255.9
	const uint32_t pwm_chanlvl = int(dac_delay / pwm_period) +1;	// Level needed to let DAC settle
	const uint32_t lm_rate = 60;	// The update rate of a line in the led matrix
	const uint32_t lm_levels = 4;	// The number of intensity levels for LEDs
	const uint32_t lm_freq = 5*lm_rate * lm_levels;	// The frequency of matrix updates
	const uint32_t lm_div = dac_rate / lm_freq;

    SPI_DAC.Init(SPI_DAC_CONF);
	//irq_set_exclusive_handler(DMA_IRQ_0, dma0_irq_handler);
	//irq_set_enabled(DMA_IRQ_0, true);

	while(!DAC.optimize_linearity(1));
	sleep_ms(10);
	while(!DAC.optimize_linearity(0));

    LM_Init();
    GO_Init();

	// Set core1 as high priority in AHB bus fabric to reduce delays
	uint32_t* busctrl_priority = (uint32_t*) BUSCTRL_BASE + 0;
	*busctrl_priority = 1 << 4;

    // Set up PWM for DAC multiplexing
	const uint32_t PWM_INH_SLICE = pwm_gpio_to_slice_num(M2IDI_MUXINH_PIN);
	const uint32_t PWM_INH_CHAN = pwm_gpio_to_channel(M2IDI_MUXINH_PIN);
    gpio_set_function(M2IDI_MUXINH_PIN, GPIO_FUNC_PWM);
    pwm_config pwm_conf = pwm_get_default_config();
    pwm_config_set_clkdiv(&pwm_conf, pwm_div);
    pwm_config_set_wrap(&pwm_conf, pwm_count-1);
    pwm_init(PWM_INH_SLICE, &pwm_conf, false);
    pwm_set_chan_level(PWM_INH_SLICE, PWM_INH_CHAN, pwm_chanlvl);
    pwm_clear_irq(PWM_INH_SLICE);
    pwm_set_irq_enabled(PWM_INH_SLICE, true);
	pwm_set_enabled(PWM_INH_SLICE, true);

    gpio_init(M2IDI_MUXA_PIN);
    gpio_init(M2IDI_MUXB_PIN);
    gpio_set_dir(M2IDI_MUXA_PIN, GPIO_OUT);
    gpio_set_dir(M2IDI_MUXB_PIN, GPIO_OUT);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, dac_pwm_handler);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    while (true){
        uint8_t next_dac = dac_processed +1;
        next_dac &= 0b11;
		DAC.update();
        if(!dac_valid){
            static uint32_t dac_count;   // Counts DAC iterations
            dac_valid = 1;
            dac_count++;
            if (dac_count >= lm_div){
                dac_count = 0;
                LM_Service();
            }
		}
        if (next_dac != dac_output){
            // Calculate a set of four values
            GO_Service(next_dac);
            dac_processed = next_dac;
        }
    }
}

void __time_critical_func(dac_pwm_handler)(){
    pwm_clear_irq(pwm_gpio_to_slice_num(M2IDI_MUXINH_PIN));
    dac_valid = 0;

    // Switch mux pin
	switch(dac_output){
		case 0:
            gpio_put(M2IDI_MUXA_PIN, 1);
            gpio_put(M2IDI_MUXB_PIN, 1);
			break;
		case 1:
            gpio_put(M2IDI_MUXA_PIN, 1);
            gpio_put(M2IDI_MUXB_PIN, 0);
			break;
		case 2:
            gpio_put(M2IDI_MUXA_PIN, 0);
            gpio_put(M2IDI_MUXB_PIN, 0);
			break;
		case 3:
            gpio_put(M2IDI_MUXA_PIN, 0);
            gpio_put(M2IDI_MUXB_PIN, 1);
			break;
	}

	uint16_t values[4];
	for (uint8_t i = 0; i < 4; i++){
		uint8_t ii = i & 1;
		values[i] = out_handler[dac_output ^ ii][i].get();
	}
	DAC.set(values, 0);
    dac_output++;
    dac_output &= 0b11;
}

void dma0_irq_handler (){
}

void __time_critical_func(dma1_irq_handler) (){
	SPI_CAN.Handler();
}
