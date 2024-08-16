/*
 * menu.cpp
 *
 * Created: 19/07/2021 17:01:37
 *  Author: GuavTek
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <hardware/timer.h>
#include "umpProcessor.h"
#include "utils.h"
#include "led_matrix.h"
#include "generic_output.h"
#include "menu.h"

menu_node_c* current_node;
generic_output_c* menu_node_c::go;
env_handler_c* menu_node_c::env;
Env_stage_t* menu_node_c::env_stage;
ctrlSource_t* menu_node_c::midi_src;
uint8_t menu_node_c::screen_change;
menu_status_t menu_node_c::status;
uint8_t menu_node_c::mem_slot;

ctrlSource_t conf_pc;
ConfigNVM_t load_backup;

uint8_t butt_state;
uint8_t butt_state_debounced;
uint32_t butt_timer;
const uint32_t butt_timeout = 20000;	// 20ms timeout

extern menu_node_c node_edit;
extern menu_bend_range_c node_edit_bend;
extern menu_select_out_c node_edit_select;
extern menu_node_c node_edit_sel_back;
extern menu_midi_out_c node_edit_sel_bind;
extern menu_key_lane_c node_edit_sel_lane;
extern menu_type_select_c node_edit_sel_type;
extern menu_type_pressure_c node_edit_sel_type_pressure;
extern menu_type_cv_c node_edit_sel_type_cv;
extern menu_type_gate_c node_edit_sel_type_gate;
extern menu_type_env_c node_edit_sel_type_envelope;
extern menu_edit_sel_env_c node_edit_sel_env_num;
extern menu_type_velocity_c node_edit_sel_type_velocity;
extern menu_type_clk_c node_edit_sel_type_clk;
extern menu_type_lfo_c node_edit_sel_type_lfo;
extern menu_lfo_shape_c node_edit_sel_type_lfo_shape;
extern menu_freq_max_c node_edit_sel_type_lfo_fmax;
extern menu_freq_min_c node_edit_sel_type_lfo_fmin;
extern menu_node_c node_edit_sel_type_lfo_back;
extern menu_node_c node_edit_sel_type_back;
extern menu_max_range_c node_edit_sel_max_range;
extern menu_min_range_c node_edit_sel_min_range;
extern menu_node_c node_edit_envelope;
extern menu_node_c node_edit_envelope_back;
extern menu_select_env_c node_edit_env;
extern menu_node_c node_edit_env_back;
extern menu_env_atk_c node_edit_env_atk;
extern menu_env_dec_c node_edit_env_dec;
extern menu_env_sus_c node_edit_env_sus;
extern menu_env_rel_c node_edit_env_rel;
extern menu_env_max_c node_edit_env_stage_max;
extern menu_env_min_c node_edit_env_stage_min;
extern menu_midi_env_c node_edit_env_stage_bind;
extern menu_node_c node_edit_env_stage_back;
extern menu_node_c node_edit_env_back;
extern menu_group_c node_edit_group;
extern menu_node_c node_edit_back;
extern menu_node_c node_save;
extern menu_mem_slot_c node_save_slot;
extern menu_midi_save_c node_save_bind_pc;
extern menu_node_c node_save_back;
extern menu_conf_save_c node_save_back_accept;
extern menu_node_c node_save_back_abort;
extern menu_conf_backup_c node_load;
extern menu_load_slot_c node_load_slot;
extern menu_node_c node_load_back;
extern menu_conf_load_c node_load_back_accept;
extern menu_restore_backup_c node_load_back_abort;

// Used to bind MIDI sources in configuration
uint8_t menu_midi(struct umpCVM* msg){
	if (current_node->handle_midi(msg)){
		needScan = true;
		return 1;
	}
	return 0;
}

void menu_init(){
	current_node = &node_edit;
	current_node->init();

	// Enable pin interrupts on buttons
	gpio_init(BUTT1);
	gpio_init(BUTT2);
	gpio_init(BUTT3);
	gpio_pull_up(BUTT1);
	gpio_pull_up(BUTT2);
	gpio_pull_up(BUTT3);
}

void check_buttons(){
	uint8_t temp_state;
	temp_state = gpio_get(BUTT1) | (gpio_get(BUTT2) << 1) | (gpio_get(BUTT3) << 2);
	if (temp_state == butt_state) {
		// inputs are debounced
		uint8_t butt_edge = butt_state_debounced & ~temp_state;
		if (butt_edge & 0b001){
			// BUTT1
			current_node->butt_right();
			needScan = true;
		}
		if (butt_edge & 0b010){
			// BUTT2
			current_node->butt_up();
			needScan = true;
		}
		if (butt_edge & 0b100){
			// BUTT3
			current_node->butt_down();
			needScan = true;
		}
		butt_state_debounced = temp_state;
	}
	butt_timer = time_us_32() + butt_timeout;
	butt_state = temp_state;
}

uint8_t menu_service(){
	if (time_us_32() > butt_timer) {
		check_buttons();
	}

	uint8_t screen_updated = current_node->screen_changed();
	current_node->update();
	return screen_updated;
}

menu_status_t get_menu_state(){
	return current_node->get_status();
}

/* current menu structure
- edit
|- bend
|- group
|- output_n
||- max_range
||- min_range
||- select_type
|||- dc
|||- gate
|||- envelope
|||- lfo
||||- shape
||||- max_frequency
||||- min_frequency
||||- return
|||- clk
|||- velocity
|||- pressure
|||- return
||- bind_midi
||- select key lane
||- return
|- edit_envelope
||- select_env
|||- attack
||||- max
||||- min
||||- bind
||||- return
|||- decay
||||- max
||||- min
||||- bind
||||- return
|||- sustain
||||- max
||||- min
||||- bind
||||- return
|||- release
||||- max
||||- min
||||- bind
||||- return
|||- return
||- return
|- return
- save
|- slot
|- bind
|- return
||- accept
||- abort
- load
|- slot
|- back
*/

struct menu_graphic_t graphic_blank = {
	line :{
		0b0000000000000000,
		0b0000000000000000,
		0b0000000000000000,
		0b0000000000000000,
		0b0000000000000000
	}
};

struct menu_graphic_t graphic_back_0 = {
	line :{
		0b0000000011000000,
		0b0000001111110000,
		0b0000110011001100,
		0b0000000011000000,
		0b0000000011111111
	}
};

struct menu_graphic_t graphic_back_1 = {
	line :{
		0b1100000011000000,
		0b0000001111110000,
		0b0000110011001100,
		0b0000000011000000,
		0b0000000011111111
	}
};

struct menu_graphic_t graphic_back_2 = {
	line :{
		0b0000000011000000,
		0b1100001111110000,
		0b0000110011001100,
		0b0000000011000000,
		0b0000000011111111
	}
};

struct menu_graphic_t graphic_back_3 = {
	line :{
		0b0000000011000000,
		0b0000001111110000,
		0b1100110011001100,
		0b0000000011000000,
		0b0000000011111111
	}
};

struct menu_graphic_t graphic_back_4 = {
	line :{
		0b0000000011000000,
		0b0000001111110000,
		0b0000110011001100,
		0b1100000011000000,
		0b0000000011111111
	}
};

struct menu_graphic_t graphic_bind_midi = {
	line :{
		0b1100000011110000,
		0b1100000011001100,
		0b1100000011110000,
		0b1100000011001100,
		0b1111110011001100
	}
};

struct menu_graphic_t graphic_accept = {
	line :{
		0b0000000000000011,
		0b0000000000001100,
		0b0011000000110000,
		0b0000110011000000,
		0b0000001100000000
	}
};

struct menu_graphic_t graphic_abort = {
	line :{
		0b0000110000001100,
		0b0000001100110000,
		0b0000000011000000,
		0b0000001100110000,
		0b0000110000001100
	}
};

struct menu_graphic_t graphic_fmax = {
	line :{
		0b1111110000110000,
		0b1100000011111100,
		0b1111001100110011,
		0b1100000000110000,
		0b1100000000110000
	}
};

struct menu_graphic_t graphic_fmin = {
	line :{
		0b1111110000110000,
		0b1100000000110000,
		0b1111001100110011,
		0b1100000011111100,
		0b1100000000110000
	}
};

struct menu_graphic_t graphic_rmax = {
	line :{
		0b1111000000110000,
		0b1100110011111100,
		0b1111001100110011,
		0b1100110000110000,
		0b1100110000110000
	}
};

struct menu_graphic_t graphic_rmin = {
	line :{
		0b1111000000110000,
		0b1100110000110000,
		0b1111001100110011,
		0b1100110011111100,
		0b1100110000110000
	}
};


// lvl0
struct menu_graphic_t graphic_edit = {
	line :{
		0b1111110011110000,
		0b1100000011001100,
		0b1111000011001100,
		0b1100000011001100,
		0b1111110011110000
	}
};

menu_node_c node_edit = menu_node_c(
	&node_edit_bend,
	&node_load,
	&node_save,
	graphic_edit
);

// lvl1
struct menu_graphic_t graphic_edit_bend = {
	line :{
		0b1111000011000011,
		0b1100110011110011,
		0b1111000011110011,
		0b1100110011001111,
		0b1111000011000011
	}
};

menu_bend_range_c node_edit_bend = menu_bend_range_c(
	&node_edit_bend,
	&node_edit_back,
	&node_edit_group,
	graphic_edit_bend
);

struct menu_graphic_t graphic_edit_group = {
	line :{
		0b1111001111001111,
		0b1111001111001111,
		0b0000000000000000,
		0b0101001111001111,
		0b0101001111001111
	}
};

menu_group_c node_edit_group = menu_group_c(
	&node_edit_group,
	&node_edit_bend,
	&node_edit_select,
	graphic_edit_group
);

menu_select_out_c node_edit_select = menu_select_out_c(
	&node_edit_sel_max_range,
	&node_edit_group,
	&node_edit_envelope,
	graphic_blank
);

// lvl2
menu_max_range_c node_edit_sel_max_range = menu_max_range_c(
	&node_edit_sel_min_range,
	&node_edit_sel_back,
	&node_edit_sel_min_range,
	graphic_rmax
);

menu_min_range_c node_edit_sel_min_range = menu_min_range_c(
	&node_edit_sel_type,
	&node_edit_sel_max_range,
	&node_edit_sel_type,
	graphic_rmin
);

struct menu_graphic_t graphic_edit_sel_type = {
	line :{
		0b1111110011110000,
		0b0011000011001100,
		0b0011000011110000,
		0b0011000011000000,
		0b0011000011000000
	}
};

menu_type_select_c node_edit_sel_type = menu_type_select_c(
	&node_edit_sel_type_cv,
	&node_edit_sel_min_range,
	&node_edit_sel_bind,
	graphic_edit_sel_type
);

// lvl3
struct menu_graphic_t graphic_edit_cv = {
	line :{
		0b0011110011001100,
		0b1100000011001100,
		0b1100000011001100,
		0b1100000000110000,
		0b0011110000110000
	}
};

menu_type_cv_c node_edit_sel_type_cv = menu_type_cv_c(
	&node_edit_sel_bind,
	&node_edit_sel_type_back,
	&node_edit_sel_type_gate,
	graphic_edit_cv
);

struct menu_graphic_t graphic_edit_gate = {
	line :{
		0b0011110000111111,
		0b1100000000001100,
		0b1100111100001100,
		0b1100001100001100,
		0b0011110000001100
	}
};

menu_type_gate_c node_edit_sel_type_gate = menu_type_gate_c(
	&node_edit_sel_bind,
	&node_edit_sel_type_cv,
	&node_edit_sel_type_envelope,
	graphic_edit_gate
);

struct menu_graphic_t graphic_edit_type_envelope = {
	line :{
		0b0000110000000000,
		0b0011001100000000,
		0b0011000011110000,
		0b1100000000001100,
		0b1100000000000011
	}
};

menu_type_env_c node_edit_sel_type_envelope = menu_type_env_c(
	&node_edit_sel_env_num,
	&node_edit_sel_type_gate,
	&node_edit_sel_type_velocity,
	graphic_edit_type_envelope
);

struct menu_graphic_t graphic_sel_envelope = {
	line :{
		0b1111110000000000,
		0b1100000000000000,
		0b1111000000000000,
		0b1100000000000000,
		0b1111110000000000
	}
};

menu_edit_sel_env_c node_edit_sel_env_num = menu_edit_sel_env_c(
	&node_edit_sel_bind,
	&node_edit_sel_env_num,
	&node_edit_sel_env_num,
	graphic_sel_envelope
);

struct menu_graphic_t graphic_edit_velocity = {
	line :{
		0b1100111111001100,
		0b1100111100001100,
		0b1100111111001100,
		0b0011001100001100,
		0b0011001111001111
	}
};

menu_type_velocity_c node_edit_sel_type_velocity = menu_type_velocity_c(
	&node_edit_sel_bind,
	&node_edit_sel_type_envelope,
	&node_edit_sel_type_pressure,
	graphic_edit_velocity
);

struct menu_graphic_t graphic_edit_pressure = {
	line :{
		0b1111001111000011,
		0b1100111100111100,
		0b1111001111000011,
		0b1100001100110011,
		0b1100001100111100
	}
};

menu_type_pressure_c node_edit_sel_type_pressure = menu_type_pressure_c(
	&node_edit_sel_bind,
	&node_edit_sel_type_velocity,
	&node_edit_sel_type_clk,
	graphic_edit_pressure
);

struct menu_graphic_t graphic_edit_clk = {
	line :{
		0b0011111100110011,
		0b1100001100110011,
		0b1100001100111100,
		0b1100001100110011,
		0b0011111111110011
	}
};

menu_type_clk_c node_edit_sel_type_clk = menu_type_clk_c(
	&node_edit_sel_bind,
	&node_edit_sel_type_pressure,
	&node_edit_sel_type_lfo,
	graphic_edit_clk
);

struct menu_graphic_t graphic_edit_lfo = {
	line :{
		0b0011111100111111,
		0b0011001100110011,
		0b0011001100110011,
		0b0011001100110011,
		0b1111001111110011
	}
};

menu_type_lfo_c node_edit_sel_type_lfo = menu_type_lfo_c(
	&node_edit_sel_type_lfo_shape,
	&node_edit_sel_type_clk,
	&node_edit_sel_type_back,
	graphic_edit_lfo
);

// lvl4
struct menu_graphic_t graphic_edit_lfo_shape = {
	line :{
		0b1111000000110000,
		0b0011000011001100,
		0b0011001100000011,
		0b0011110000000000,
		0b0011000000000000
	}
};

menu_lfo_shape_c node_edit_sel_type_lfo_shape = menu_lfo_shape_c(
	&node_edit_sel_type_lfo_fmax,
	&node_edit_sel_type_lfo_back,
	&node_edit_sel_type_lfo_fmax,
	graphic_edit_lfo_shape
);

menu_freq_max_c node_edit_sel_type_lfo_fmax = menu_freq_max_c(
	&node_edit_sel_type_lfo_fmin,
	&node_edit_sel_type_lfo_shape,
	&node_edit_sel_type_lfo_fmin,
	graphic_fmax
);

menu_freq_min_c node_edit_sel_type_lfo_fmin = menu_freq_min_c(
	&node_edit_sel_type_lfo_back,
	&node_edit_sel_type_lfo_fmax,
	&node_edit_sel_type_lfo_back,
	graphic_fmin
);

menu_node_c node_edit_sel_type_lfo_back = menu_node_c(
	&node_edit_sel_bind,
	&node_edit_sel_type_lfo_fmin,
	&node_edit_sel_type_lfo_shape,
	graphic_back_3
);

// lvl3
menu_node_c node_edit_sel_type_back = menu_node_c(
	&node_edit_sel_back,
	&node_edit_sel_type_lfo,
	&node_edit_sel_type_cv,
	graphic_back_2
);

// lvl2
menu_midi_out_c node_edit_sel_bind = menu_midi_out_c(
	&node_edit_sel_lane,
	&node_edit_sel_type,
	&node_edit_sel_lane,
	graphic_bind_midi
);

struct menu_graphic_t graphic_edit_key_lane = {
	line :{
		0b1100000011000000,
		0b1100110011000000,
		0b1111000011000000,
		0b1100110011000000,
		0b1100110011111100
	}
};

menu_key_lane_c node_edit_sel_lane = menu_key_lane_c(
	&node_edit_sel_back,
	&node_edit_sel_bind,
	&node_edit_sel_back,
	graphic_edit_key_lane
);

menu_node_c node_edit_sel_back = menu_node_c(
	&node_edit_select,
	&node_edit_sel_lane,
	&node_edit_sel_max_range,
	graphic_back_1
);

// lvl1
struct menu_graphic_t graphic_edit_envelope = {
	line :{
		0b0000110000000000,
		0b0011001111000000,
		0b0011000000110000,
		0b1100000000001100,
		0b1100000000000011
	}
};

menu_node_c node_edit_envelope = menu_node_c(
	&node_edit_env,
	&node_edit_select,
	&node_edit_back,
	graphic_edit_envelope
);

// lvl2
menu_select_env_c node_edit_env = menu_select_env_c(
	&node_edit_env_atk,
	&node_edit_envelope_back,
	&node_edit_envelope_back,
	graphic_sel_envelope
);

menu_node_c node_edit_envelope_back = menu_node_c(
	&node_edit_envelope,
	&node_edit_env,
	&node_edit_env,
	graphic_back_1
);

// lvl3
struct menu_graphic_t graphic_env_atk = {
	line :{
		0b0000000000001100,
		0b0000000000110011,
		0b0000000011000000,
		0b0000001100000000,
		0b0000110000000000
	}
};

menu_env_atk_c node_edit_env_atk = menu_env_atk_c(
	&node_edit_env_stage_max,
	&node_edit_env_back,
	&node_edit_env_dec,
	graphic_env_atk
);

struct menu_graphic_t graphic_env_dec = {
	line :{
		0b0011000000000000,
		0b1100110000000000,
		0b1100001100000000,
		0b0000000011000000,
		0b0000000000111111
	}
};

menu_env_dec_c node_edit_env_dec = menu_env_dec_c(
	&node_edit_env_stage_max,
	&node_edit_env_atk,
	&node_edit_env_sus,
	graphic_env_dec
);

struct menu_graphic_t graphic_env_sus = {
	line :{
		0b1100000000000000,
		0b0011000000000000,
		0b0000111111110000,
		0b0000000000001100,
		0b0000000000000011
	}
};

menu_env_sus_c node_edit_env_sus = menu_env_sus_c(
	&node_edit_env_stage_max,
	&node_edit_env_dec,
	&node_edit_env_rel,
	graphic_env_sus
);

struct menu_graphic_t graphic_env_rel = {
	line :{
		0b0000000000000000,
		0b1111110000000000,
		0b0000001100000000,
		0b0000000011000000,
		0b0000000000110000
	}
};

menu_env_rel_c node_edit_env_rel = menu_env_rel_c(
	&node_edit_env_stage_max,
	&node_edit_env_sus,
	&node_edit_env_back,
	graphic_env_rel
);

// lvl4
menu_env_max_c node_edit_env_stage_max = menu_env_max_c(
	&node_edit_env_stage_min,
	&node_edit_env_stage_back,
	&node_edit_env_stage_min,
	graphic_rmax
);

menu_env_min_c node_edit_env_stage_min = menu_env_min_c(
	&node_edit_env_stage_bind,
	&node_edit_env_stage_max,
	&node_edit_env_stage_bind,
	graphic_rmin
);

menu_midi_env_c node_edit_env_stage_bind = menu_midi_env_c(
	&node_edit_env_stage_back,
	&node_edit_env_stage_min,
	&node_edit_env_stage_back,
	graphic_bind_midi
);

menu_node_c node_edit_env_stage_back = menu_node_c(
	&node_edit_env_back,
	&node_edit_env_stage_bind,
	&node_edit_env_stage_max,
	graphic_back_2
);

// lvl3
menu_node_c node_edit_env_back = menu_node_c(
	&node_edit_envelope,
	&node_edit_env_rel,
	&node_edit_env_atk,
	graphic_back_1
);

// lvl1
menu_node_c node_edit_back = menu_node_c(
	&node_edit,
	&node_edit_envelope,
	&node_edit_bend,
	graphic_back_0
);

// lvl0
struct menu_graphic_t graphic_save = {
	line :{
		0b0011111100000011,
		0b1100001100000011,
		0b0011000011001100,
		0b0000110011001100,
		0b1111000000110000
	}
};

menu_node_c node_save = menu_node_c(
	&node_save_slot,
	&node_edit,
	&node_load,
	graphic_save
);

// lvl1
struct menu_graphic_t graphic_save_slot = {
	line :{
		0b0000000011111111,
		0b0000000011111111,
		0b0000000000000011,
		0b0000000011111111,
		0b0000000011111111
	}
};

menu_mem_slot_c node_save_slot = menu_mem_slot_c(
	&node_save_bind_pc,
	&node_save_back,
	&node_save_bind_pc,
	graphic_save_slot
);

struct menu_graphic_t graphic_bind_pc = {
	line :{
		0b1111000000111100,
		0b1100110011000000,
		0b1111000011000000,
		0b1100000011000000,
		0b1100000000111100
	}
};

menu_midi_save_c node_save_bind_pc = menu_midi_save_c(
	&node_save_back,
	&node_save_slot,
	&node_save_back,
	graphic_bind_pc
);

menu_node_c node_save_back = menu_node_c(
	&node_save_back_accept,
	&node_save_bind_pc,
	&node_save_slot,
	graphic_back_0
);

// lvl2
menu_conf_save_c node_save_back_accept = menu_conf_save_c(
	&node_save,
	&node_save_back_abort,
	&node_save_back_abort,
	graphic_accept
);

menu_node_c node_save_back_abort = menu_node_c(
	&node_save,
	&node_save_back_accept,
	&node_save_back_accept,
	graphic_abort
);

// lvl0
struct menu_graphic_t graphic_load = {
	line :{
		0b1100000011110000,
		0b1100000011001100,
		0b1100000011001100,
		0b1100000011001100,
		0b1111110011110000
	}
};

menu_conf_backup_c node_load = menu_conf_backup_c(
	&node_load_slot,
	&node_save,
	&node_edit,
	graphic_load
);

// lvl0
struct menu_graphic_t graphic_load_slot = {
	line :{
		0b0000000011111111,
		0b0000000011111111,
		0b0000000000000011,
		0b0000000011111111,
		0b0000000011111111
	}
};

// lvl1
menu_load_slot_c node_load_slot = menu_load_slot_c(
	&node_load_back_accept,
	&node_load_back,
	&node_load_back,
	graphic_load_slot
);

menu_node_c node_load_back = menu_node_c(
	&node_load,
	&node_load_slot,
	&node_load_slot,
	graphic_back_0
);

menu_conf_load_c node_load_back_accept = menu_conf_load_c(
	&node_load,
	&node_load_back_abort,
	&node_load_back_abort,
	graphic_accept
);

menu_restore_backup_c node_load_back_abort = menu_restore_backup_c(
	&node_load,
	&node_load_back_accept,
	&node_load_back_accept,
	graphic_abort
);

// base node
void menu_node_c::init(){
	status = menu_status_t::Navigate;
	screen_change = 1;
}
void menu_node_c::update(){
	if (!screen_change) return;
	screen_change = 0;
	for (uint8_t i = 0; i < 5; i++)	{
		LM_WriteRow(i, graphic.line[i]);
	}
}

void menu_node_c::butt_up(){
	current_node = node_next;
	current_node->init();
}

void menu_node_c::butt_down(){
	current_node = node_prev;
	current_node->init();
}

void menu_node_c::butt_right(){
	current_node = node_in;
	current_node->init();
}

// mem conf nodes
void menu_conf_save_c::butt_right(){
	if (conf_pc.sourceType == ctrlType_t::none){
		mem_write_config(mem_slot, -1);
	} else {
		mem_write_config(mem_slot, conf_pc.sourceNum);
	}
	menu_node_c::butt_right();
}

void menu_conf_load_c::butt_right(){
	mem_confirm_load(mem_slot);
	menu_node_c::butt_right();
}

void menu_conf_backup_c::butt_right(){
	GO_Get_Config(&load_backup);
	menu_node_c::butt_right();
}

void menu_restore_backup_c::butt_right(){
	GO_Set_Config(&load_backup);
	menu_node_c::butt_right();
}

void menu_midi_save_c::init(){
		menu_wait_midi_c::init();
		midi_src = &conf_pc;
		midi_mask = 0b100;
}

// int node
void menu_int_c::update(){
	if (status == menu_status_t::Edit_int) {
		if (!screen_change) return;
		screen_change = 0;
		uint8_t temp_value = get_value();
		uint8_t lo = temp_value & 0b111;
		uint8_t mid = (temp_value >> 3) & 0b111;
		uint8_t hi = temp_value >> 6;
		LM_WriteRow(2, 0xffff >> (16-2*hi));
		LM_WriteRow(3, 0xffff >> (16-2*mid));
		LM_WriteRow(4, 0xffff >> (14-2*lo));
	} else {
		menu_node_c::update();
	}
}

void menu_int_c::butt_up(){
	if (status == menu_status_t::Edit_int){
		uint32_t temp_value = get_value();
		temp_value++;
		set_value(temp_value);
		screen_change = 1;
	} else {
		menu_node_c::butt_up();
	}
}

void menu_int_c::butt_down(){
	if (status == menu_status_t::Edit_int){
		uint32_t temp_value = get_value();
		temp_value--;
		set_value(temp_value);
		screen_change = 1;
	} else {
		menu_node_c::butt_down();
	}
}

void menu_int_c::butt_right(){
	if (status == menu_status_t::Edit_int) {
		menu_node_c::butt_right();
	} else {
		status = menu_status_t::Edit_int;
		screen_change = 1;
		LM_WriteRow(0, 0xffff);
		LM_WriteRow(1, 0);
	}
}

// LFO shape node
void menu_lfo_shape_c::init(){
	status = menu_status_t::SetLFO;
	screen_change = 1;
}

void menu_lfo_shape_c::update(){
	if (status == menu_status_t::Edit_int) {
		if (!screen_change) return;
		screen_change = 0;
		WavShape_t tempShape = (WavShape_t) get_value();
		switch(tempShape){
			case WavShape_t::Square:
				LM_WriteRow(0, 0b0011111111000011);
				LM_WriteRow(1, 0b0011000011000011);
				LM_WriteRow(2, 0b0011000011000011);
				LM_WriteRow(3, 0b0011000011000011);
				LM_WriteRow(4, 0b1111000011111111);
				break;
			case WavShape_t::Triangle:
				LM_WriteRow(0, 0b0000000011000000);
				LM_WriteRow(1, 0b0000001100110000);
				LM_WriteRow(2, 0b0000110000001100);
				LM_WriteRow(3, 0b0011000000000011);
				LM_WriteRow(4, 0b1100000000000000);
				break;
			case WavShape_t::Sawtooth:
				LM_WriteRow(0, 0b1100000011000000);
				LM_WriteRow(1, 0b1111000011110000);
				LM_WriteRow(2, 0b1100110011001100);
				LM_WriteRow(3, 0b1100001111000011);
				LM_WriteRow(4, 0b1100000011000000);
				break;
			case WavShape_t::Sine:
				LM_WriteRow(0, 0b0000000000000011);
				LM_WriteRow(1, 0b0000000000111100);
				LM_WriteRow(2, 0b0000000000110000);
				LM_WriteRow(3, 0b1100000011110000);
				LM_WriteRow(4, 0b0011111100000000);
				break;
			case WavShape_t::SinSaw:
				LM_WriteRow(0, 0b1111000000111100);
				LM_WriteRow(1, 0b1100110000110011);
				LM_WriteRow(2, 0b1100110000110000);
				LM_WriteRow(3, 0b1100001100110000);
				LM_WriteRow(4, 0b1100000011110000);
				break;
			case WavShape_t::SuperSaw:
				LM_WriteRow(0, 0b1100000000000000);
				LM_WriteRow(1, 0b1111001100000011);
				LM_WriteRow(2, 0b1100110011000011);
				LM_WriteRow(3, 0b1100000000110011);
				LM_WriteRow(4, 0b0000000000001111);
				break;
			case WavShape_t::Noise:
				LM_WriteRow(0, 0b0000000000110000);
				LM_WriteRow(1, 0b0011001100110011);
				LM_WriteRow(2, 0b1111111111111111);
				LM_WriteRow(3, 0b0000110011000000);
				LM_WriteRow(4, 0b0000110000000000);
				break;
		}
	} else {
		menu_node_c::update();
	}
}

void menu_lfo_shape_c::set_value(uint32_t val){
	if (val >= 255){
		val = 4;
	} else if (val >= 7){
		val = 0;
	}
	go->state.shape = (WavShape_t) val;
}

uint32_t menu_lfo_shape_c::get_value(){
	return (uint32_t) go->state.shape;
}

// MIDI node
void menu_wait_midi_c::update(){
	if (status == menu_status_t::Wait_MIDI) {
		uint8_t timer = (time_us_32() >> 18) & 0b1111;
		LM_WriteRow(3, 0b0011001100110011 & ~(0b11 << timer));
	} else {
		menu_node_c::update();
	}
}

void menu_wait_midi_c::butt_right(){
	if (status == menu_status_t::Wait_MIDI){
		midi_src->sourceType = ctrlType_t::none;
		menu_node_c::butt_right();
	} else {
		status = menu_status_t::Wait_MIDI;
		LM_WriteRow(0,0xffff);
		LM_WriteRow(1,0);
		LM_WriteRow(2,0);
		LM_WriteRow(4,0);
		screen_change = 1;
	}
}

uint8_t menu_wait_midi_c::handle_midi(struct umpCVM* msg){
	if (status != menu_status_t::Wait_MIDI) return 0;
	switch(msg->status){
		case PROGRAM_CHANGE:
			if (!(midi_mask & 0b100)){
				return 0;
			}
			// TODO: Set global bank?
			midi_src->sourceType = ctrlType_t::program;
			midi_src->sourceNum = msg->value;
			break;
		case NOTE_ON:
		case NOTE_OFF:
			if (!(midi_mask & 0b010)){
				return 0;
			}
			midi_src->sourceType = ctrlType_t::key;
			midi_src->sourceNum = msg->note;
			break;
		case CC:
			if (!(midi_mask & 0b001)){
				return 0;
			}
			midi_src->sourceType = ctrlType_t::controller;
			midi_src->sourceNum = msg->index;
			break;
		default:
			return 0;
	}
	midi_src->channel = msg->channel;
	menu_node_c::butt_right();
	return 1;
}

// edit 32-bit node
void menu_32bit_c::update(){
	if (status == menu_status_t::Navigate) {
		menu_node_c::update();
	} else {
		if (!screen_change) return;
		screen_change = 0;
		uint16_t disp_line[4];
		uint32_t temp_expand = get_value();
		for (uint8_t i = 0; i < 4; i++){
			disp_line[i] = 0;
			for (uint8_t n = 0; n < 16; n += 2){
				uint8_t temp = temp_expand & 1;
				temp |= temp << 1;
				temp_expand >>= 1;
				disp_line[i] |= temp << n;
			}
		}
		LM_WriteRow(3, disp_line[0]);
		LM_WriteRow(2, disp_line[1]);
		LM_WriteRow(1, disp_line[2]);
		LM_WriteRow(0, disp_line[3]);
	}
}

uint32_t menu_32bit_c::extrapolate_num (uint32_t in){
	uint32_t tempResult = in & (0xffff'ffff << bit_pos);
	uint8_t cleanIn = (in >> bit_pos) & 0xf;
	int8_t i = bit_pos - 4;
	for (; i > 0; i -= 4){
		tempResult |= cleanIn << i;
	}
	tempResult |= cleanIn >> -i;
	return tempResult;
}

void menu_32bit_c::butt_up(){
	screen_change = 1;
	if (status == menu_status_t::Edit_major_bit) {
		bit_pos++;
		if (bit_pos >= num_bits()) {
			bit_pos = 0;
		}
		set_value(1 << bit_pos);
	} else if (status == menu_status_t::Edit_minor_bit) {
		uint32_t temp_val = get_value();
		temp_val += 1 << bit_pos;
		temp_val &= 0xffff'ffff >> (32-num_bits());
		temp_val = extrapolate_num(temp_val);
		set_value(temp_val);
	} else {
		menu_node_c::butt_up();
	}
}

void menu_32bit_c::butt_down(){
	screen_change = 1;
	if (status == menu_status_t::Edit_major_bit) {
		bit_pos--;
		if (bit_pos < 0){
			bit_pos = num_bits()-1;
		}
		set_value(1 << bit_pos);
	} else if (status == menu_status_t::Edit_minor_bit) {
		uint32_t temp_val = get_value();
		temp_val -= 1 << bit_pos;
		temp_val &= 0xffff'ffff >> (32-num_bits());
		temp_val = extrapolate_num(temp_val);
		set_value(temp_val);
	} else {
		menu_node_c::butt_down();
	}
}

void menu_32bit_c::butt_right(){
	if (status == menu_status_t::Edit_minor_bit){
		menu_node_c::butt_right();
	} else if (status == menu_status_t::Edit_major_bit) {
		if (bit_pos > 3){
			bit_pos -= 3;
		} else {
			bit_pos = 0;
		}
		status = menu_status_t::Edit_minor_bit;
	} else {
		status = menu_status_t::Edit_major_bit;
		screen_change = 1;
		LM_WriteRow(4, 0xffff);
		// Find current MSb
		uint32_t temp_val = get_value();
		bit_pos = 0;
		for (int8_t i = num_bits(); i > 0; --i){
			if (temp_val & (1 << i)){
				bit_pos = i;
				break;
			}
		}
	}
}

// edit 16-bit node
void menu_16bit_c::update(){
	if (status == menu_status_t::Navigate){
		menu_node_c::update();
	} else {
		if (!screen_change) return;
		screen_change = 0;
		uint16_t disp_line[2];
		uint16_t temp_expand = get_value();
		for (uint8_t i = 0; i < 2; i++){
			disp_line[i] = 0;
			for (uint8_t n = 0; n < 16; n += 2){
				uint8_t temp = temp_expand & 1;
				temp |= temp << 1;
				temp_expand >>= 1;
				disp_line[i] |= temp << n;
			}
		}
		LM_WriteRow(3, disp_line[0]);
		LM_WriteRow(2, disp_line[1]);
		LM_WriteRow(1, disp_line[0]);
		LM_WriteRow(0, disp_line[1]);
	}
}

// edit 8-bit node
void menu_8bit_c::update(){
	if (status == menu_status_t::Navigate){
		menu_node_c::update();
	} else {
		if (!screen_change) return;
		screen_change = 0;
		uint16_t disp_line;
		uint8_t temp_expand = get_value();
		disp_line = 0;
		for (uint8_t n = 0; n < 16; n += 2){
			uint8_t temp = temp_expand & 1;
			temp |= temp << 1;
			temp_expand >>= 1;
			disp_line |= temp << n;
		}
		LM_WriteRow(3, disp_line);
		LM_WriteRow(2, disp_line);
		LM_WriteRow(1, disp_line);
		LM_WriteRow(0, disp_line);
	}
}

// output select node
void menu_select_out_c::init(){
	num_out = 0;
	go = &out_handler[0][0];
	menu_node_c::init();
}

void menu_select_out_c::update(){
	if (!screen_change) return;
	screen_change = 0;
	uint8_t x = num_out & 0b11;
	uint8_t y = num_out >> 2;
	LM_WriteRow(4, 0xffff);
	for (uint8_t i = 0; i < 4; i++){
		LM_WriteRow(i, 0b1111000000001111 | ((x == i) ? 0b10 << (2*(5 - y)) : 0));
	}
}

void menu_select_out_c::butt_up(){
	screen_change = 1;
	if (num_out > 0){
		num_out--;
		uint8_t x = num_out & 0b11;
		uint8_t y = num_out >> 2;
		go = &out_handler[x][y];
	} else {
		menu_node_c::butt_up();
	}
}

void menu_select_out_c::butt_down(){
	screen_change = 1;
	if (num_out < 15){
		num_out++;
		uint8_t x = num_out & 0b11;
		uint8_t y = num_out >> 2;
		go = &out_handler[x][y];
	} else {
		menu_node_c::butt_down();
	}
}

// envelope select node
void menu_select_env_c::init(){
	num_out = 0;
	menu_node_c::init();
}

void menu_select_env_c::update(){
	if (!screen_change) return;
	screen_change = 0;
	uint16_t lines[5];
	if (num_out == 0){
		lines[0] = graphic.line[0] | 0b00001100;
		lines[1] = graphic.line[1] | 0b00110011;
		lines[2] = graphic.line[2] | 0b00110011;
		lines[3] = graphic.line[3] | 0b00110011;
		lines[4] = graphic.line[4] | 0b00001100;
	} else if (num_out == 1){
		lines[0] = graphic.line[0] | 0b00001100;
		lines[1] = graphic.line[1] | 0b00001100;
		lines[2] = graphic.line[2] | 0b00001100;
		lines[3] = graphic.line[3] | 0b00001100;
		lines[4] = graphic.line[4] | 0b00001100;
	} else if (num_out == 2){
		lines[0] = graphic.line[0] | 0b00111100;
		lines[1] = graphic.line[1] | 0b00000011;
		lines[2] = graphic.line[2] | 0b00001100;
		lines[3] = graphic.line[3] | 0b00110000;
		lines[4] = graphic.line[4] | 0b00111111;
	} else if (num_out == 3){
		lines[0] = graphic.line[0] | 0b00111100;
		lines[1] = graphic.line[1] | 0b00000011;
		lines[2] = graphic.line[2] | 0b00111100;
		lines[3] = graphic.line[3] | 0b00000011;
		lines[4] = graphic.line[4] | 0b00111100;
	}
	for (uint8_t i = 0; i < 5; i++){
		LM_WriteRow(i, lines[i]);
	}
}

void menu_select_env_c::butt_up(){
	screen_change = 1;
	if (num_out < 3){
		num_out++;
	} else {
		menu_node_c::butt_up();
	}
}

void menu_select_env_c::butt_down(){
	screen_change = 1;
	if (num_out > 0){
		num_out--;
	} else {
		menu_node_c::butt_down();
	}
}

void menu_select_env_c::butt_right(){
	env = &envelopes[num_out];
	menu_node_c::butt_right();
}

// type select node
void menu_type_select_c::butt_right(){
	switch (go->state.type){
	case GOType_t::DC:
		current_node = &node_edit_sel_type_cv;
		break;
	case GOType_t::Gate:
		current_node = &node_edit_sel_type_gate;
		break;
	case GOType_t::CLK:
		current_node = &node_edit_sel_type_clk;
		break;
	case GOType_t::Envelope:
		current_node = &node_edit_sel_type_envelope;
		break;
	case GOType_t::LFO:
		current_node = &node_edit_sel_type_lfo;
		break;
	case GOType_t::Pressure:
		current_node = &node_edit_sel_type_pressure;
		break;
	case GOType_t::Velocity:
		current_node = &node_edit_sel_type_velocity;
		break;
	default:
		break;
	}
	current_node->init();
}
