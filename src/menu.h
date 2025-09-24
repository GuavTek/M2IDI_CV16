/*
 * menu.h
 *
 * Created: 19/07/2021 17:01:16
 *  Author: GuavTek
 */


#ifndef MENU_H_
#define MENU_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "umpProcessor.h"
#include "utils.h"
#include "memory.h"

struct menu_graphic_t {
	const uint16_t line[5];
};

enum menu_status_t {
	Navigate,
	SetLFO,
	Edit_minor_bit,
	Edit_major_bit,
	Edit_int,
	Wait_MIDI
};

extern bool block_conf_pc;

void menu_init();
uint8_t menu_service();
uint8_t menu_midi(struct umpCVM* msg);

menu_status_t get_menu_state();

// Base node with navigation
class menu_node_c {
public:
	menu_node_c(menu_node_c* in, menu_node_c* prev, menu_node_c* next, const menu_graphic_t graph) : graphic(graph) {
		node_prev = prev;
		node_next = next;
		node_in = in;
	}
	~menu_node_c(){}
	inline uint8_t screen_changed(){return screen_change;}
	inline menu_status_t get_status(){return status;}
	virtual void init();
	virtual void update();
	virtual void butt_up();
	virtual void butt_down();
	virtual void butt_right();
	virtual uint8_t handle_midi(struct umpCVM* msg){return 0;}
protected:
	virtual void set_value(uint32_t val){}
	virtual uint32_t get_value(){return 0;}
	menu_node_c* node_prev;
	menu_node_c* node_next;
	menu_node_c* node_in;
	static generic_output_c* go;
	static env_handler_c* env;
	static Env_stage_t* env_stage;
	static ctrlSource_t* midi_src;
	static uint8_t screen_change;
	static menu_status_t status;
	static uint8_t mem_slot;
	const menu_graphic_t graphic;
};

class menu_type_select_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right();
};

class menu_type_cv_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		go->set_type(GOType_t::DC);
		menu_node_c::butt_right();
	}
};

class menu_type_pressure_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		go->set_type(GOType_t::Pressure);
		menu_node_c::butt_right();
	}
};

class menu_type_gate_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		go->set_type(GOType_t::Gate);
		menu_node_c::butt_right();
	}
};

class menu_type_env_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		go->set_type(GOType_t::Envelope);
		menu_node_c::butt_right();
	}
};

class menu_type_velocity_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		go->set_type(GOType_t::Velocity);
		menu_node_c::butt_right();
	}
};

class menu_type_lfo_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		go->set_type(GOType_t::LFO);
		menu_node_c::butt_right();
	}
};

class menu_env_atk_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		env_stage = &env->env.att;
		menu_node_c::butt_right();
	}
};

class menu_env_dec_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		env_stage = &env->env.dec;
		menu_node_c::butt_right();
	}
};

class menu_env_sus_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		env_stage = &env->env.sus;
		menu_node_c::butt_right();
	}
};

class menu_env_rel_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right(){
		env_stage = &env->env.rel;
		menu_node_c::butt_right();
	}
};

class menu_block_pc_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void init(){
		block_conf_pc = 0;
		menu_node_c::init();
	}
	virtual void butt_right(){
		block_conf_pc = 1;
		menu_node_c::butt_right();
	}
};

class menu_conf_save_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right();
};

class menu_conf_load_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right();
};

class menu_conf_backup_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void init(){
		block_conf_pc = 0;
		menu_node_c::init();
	}
	virtual void butt_right();
};

class menu_restore_backup_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void butt_right();
};

class menu_int_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void update();
	virtual void butt_up();
	virtual void butt_down();
	virtual void butt_right();
};

class menu_lfo_shape_c : public menu_int_c {
public:
	using menu_int_c::menu_int_c;
	virtual void init();
	virtual void update();
protected:
	virtual void set_value(uint32_t val);
	virtual uint32_t get_value();
};

class menu_bend_range_c : public menu_int_c {
public:
	using menu_int_c::menu_int_c;
protected:
	virtual void set_value(uint32_t val){
		if (val > 250){
			val = 12;
		} else if (val > 12) {
			val = 0;
		}
		key_handler.set_bend_range(val);
	}
	virtual uint32_t get_value(){return key_handler.get_current_bend();}
};

class menu_group_c : public menu_int_c {
public:
	using menu_int_c::menu_int_c;
protected:
	virtual void set_value(uint32_t val){
		if (val > 250){
			val = 15;
		} else if (val > 15) {
			val = 0;
		}
		midi_group = val;
	}
	virtual uint32_t get_value(){return midi_group;}
};

class menu_key_lane_c : public menu_int_c {
public:
	using menu_int_c::menu_int_c;
protected:
	virtual void set_value(uint32_t val){
		if (val > 250){
			val = 8;
		} else if (val > 8) {
			val = 0;
		}
		go->state.key_lane = val;
	}
	virtual uint32_t get_value(){return go->state.key_lane;}
};

class menu_mem_slot_c : public menu_int_c {
public:
	using menu_int_c::menu_int_c;
protected:
	virtual void set_value(uint32_t val){
		if (val > 250){
			val = MAX_MEM_SLOT-1;
		} else if (val >= MAX_MEM_SLOT) {
			val = 0;
		}
		mem_slot = val;
	}
	virtual uint32_t get_value(){return mem_slot;}
};

class menu_load_slot_c : public menu_mem_slot_c {
public:
	using menu_mem_slot_c::menu_mem_slot_c;
protected:
	virtual void set_value(uint32_t val){
		menu_mem_slot_c::set_value(val);
		mem_read_config(mem_slot);
	}
};

class menu_wait_midi_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void update();
	virtual void butt_right();
	virtual uint8_t handle_midi(struct umpCVM* msg);
protected:
	uint8_t midi_mask;
};

class menu_midi_env_c : public menu_wait_midi_c{
public:
	using menu_wait_midi_c::menu_wait_midi_c;
	virtual void init(){
		menu_wait_midi_c::init();
		midi_src = &env_stage->source;
		midi_mask = 0b011;
	}
};

class menu_midi_mod_c : public menu_wait_midi_c{
public:
	using menu_wait_midi_c::menu_wait_midi_c;
	virtual void init(){
		menu_wait_midi_c::init();
		midi_src = &go->state.mod_source;
		midi_mask = 0b001;
	}
};

class menu_midi_out_c : public menu_wait_midi_c{
public:
	using menu_wait_midi_c::menu_wait_midi_c;
	virtual void init(){
		menu_wait_midi_c::init();
		midi_src = &go->state.gen_source;
		midi_mask = 0b111;
	}
};

class menu_midi_save_c : public menu_wait_midi_c{
public:
	using menu_wait_midi_c::menu_wait_midi_c;
	virtual void init();
};

class menu_bool_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void update();
	virtual void butt_right();
};

class menu_env_disable_c : public menu_bool_c {
public:
	using menu_bool_c::menu_bool_c;
protected:
	virtual uint32_t get_value(){return env_stage->disable;}
	virtual void set_value(uint32_t val){
		env_stage->disable = val;
	}
};

class menu_32bit_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void update();
	virtual void butt_up();
	virtual void butt_down();
	virtual void butt_right();
protected:
	int8_t bit_pos;
	virtual uint8_t num_bits(){return 32;}
	uint32_t extrapolate_num(uint32_t in);
};

class menu_env_max_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
protected:
	virtual uint32_t get_value(){return env_stage->max;}
	virtual void set_value(uint32_t val){
		env_stage->max = val;
		env_stage->current = val;
	}
};

class menu_env_min_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
protected:
	virtual uint32_t get_value(){return env_stage->min;}
	virtual void set_value(uint32_t val){
		env_stage->min = val;
		env_stage->current = val;
	}
};

class menu_freq_max_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
protected:
	virtual uint32_t get_value(){return go->state.freq_max;}
	virtual void set_value(uint32_t val){
		go->state.freq_max = val;
		go->state.freq_current = val;
	}
};

class menu_freq_min_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
protected:
	virtual uint32_t get_value(){return go->state.freq_min;}
	virtual void set_value(uint32_t val){
		go->state.freq_min = val;
		go->state.freq_current = val;
	}
};

class menu_mod_max_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
protected:
	virtual uint32_t get_value(){return go->state.mod_max;}
	virtual void set_value(uint32_t val){
		go->state.mod_max = val;
		go->state.mod_current = val;
	}
};

class menu_mod_min_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
protected:
	virtual uint32_t get_value(){return go->state.mod_min;}
	virtual void set_value(uint32_t val){
		go->state.mod_min = val;
		go->state.mod_current = val;
	}
};

class menu_16bit_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
	virtual void update();
protected:
	virtual uint8_t num_bits(){return 16;}
};

class menu_max_range_c : public menu_16bit_c {
public:
	using menu_16bit_c::menu_16bit_c;
protected:
	virtual uint32_t get_value(){return go->state.max_range;}
	virtual void set_value(uint32_t val){
		go->state.max_range = val;
		go->state.currentOut = val;
	}
};

class menu_min_range_c : public menu_16bit_c {
public:
	using menu_16bit_c::menu_16bit_c;
protected:
	virtual uint32_t get_value(){return go->state.min_range;}
	virtual void set_value(uint32_t val){
		go->state.min_range = val;
		go->state.currentOut = val;
	}
};

class menu_8bit_c : public menu_32bit_c {
public:
	using menu_32bit_c::menu_32bit_c;
	virtual void update();
protected:
	virtual uint8_t num_bits(){return 8;}
};

class menu_type_clk_c : public menu_8bit_c {
public:
	using menu_8bit_c::menu_8bit_c;
	virtual void butt_right(){
		if (status == menu_status_t::Navigate){
			go->set_type(GOType_t::CLK);
		}
		menu_8bit_c::butt_right();
	}
protected:
	virtual uint32_t get_value(){return go->state.freq_current;}
	virtual void set_value(uint32_t val){
		go->state.freq_current = val;
		go->state.freq_max = val;
		go->state.freq_min = val;
	}
};

class menu_select_out_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void init();
	virtual void update();
	virtual void butt_up();
	virtual void butt_down();
protected:
	uint8_t num_out;
};

class menu_select_env_c : public menu_node_c {
public:
	using menu_node_c::menu_node_c;
	virtual void init();
	virtual void update();
	virtual void butt_up();
	virtual void butt_down();
	virtual void butt_right();
protected:
	uint8_t num_out;
};

class menu_edit_sel_env_c : public menu_select_env_c {
public:
	using menu_select_env_c::menu_select_env_c;
	virtual void butt_right(){
		go->state.env_num = num_out;
		menu_node_c::butt_right();
	}
};

#endif /* MENU_H_ */