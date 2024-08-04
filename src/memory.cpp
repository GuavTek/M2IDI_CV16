#include "memory.h"

const uint32_t MAX_MEM_SLOT = ((RAM_SIZE-0x300-256*sizeof(ctrlSource_t))/sizeof(ConfigNVM_t));

eeprom_cat_c* mem_handler;
ConfigNVM_t mem_buff;
uint32_t slot_pc[128];
char head_buff[4];

// Pending writes
int8_t pend_slot;
bool pend_pc = false;
bool pend_conf = false;
bool pend_read = false;
bool pend_head = false;
bool is_reading = false;

void mem_update();

void mem_init(){
	mem_handler->init(EEPROM_CONF, EEPROM_SECTIONS, 4);
	mem_handler->set_callback(mem_update);

	while(mem_handler->is_busy());
	// Memory layout version
	mem_handler->read_data(head_buff, 0, 0);
	while(mem_handler->is_busy());
	if (head_buff[0] != 1){
		// Initialize header
		// Set version number
		head_buff[0] = 1;
		head_buff[1] = 0;
		head_buff[2] = 0;
		head_buff[3] = 0;
		mem_handler->write_data(head_buff, 0, 0);
		while(mem_handler->is_busy());
		// Set previous config as none
		head_buff[0] = 250;
		mem_handler->write_data(head_buff, 0, 1);
		while(mem_handler->is_busy());
		// load default config
		GO_Default_Config();
	} else {
		// Fetch other header data
		mem_handler->read_data(head_buff, 0, 1);
		while(mem_handler->is_busy());
		if (head_buff[0] == 250){
			// No previous config, load default
			GO_Default_Config();
		} else {
			// Load last used config for outputs
			mem_read_config(head_buff[0]);
			while(mem_handler->is_busy());
		}

	}
	// TODO: load CC table
	// load slots PC lookup
	mem_handler->read_items((char*) slot_pc, 2, 0, 128);
	while(mem_handler->is_busy());
}

void mem_read_config(uint8_t slot_num){
	pend_slot = slot_num;
	if (mem_handler->read_data((char*) &mem_buff, 3, slot_num)) {
		is_reading = true;
	} else {
		pend_read = true;
	}
}

void mem_confirm_load(uint8_t slot_num){
	// Update header config on manual load
	head_buff[0] = pend_slot;
	if (!mem_handler->write_data(head_buff, 0, 1)) {
		pend_head = true;
		pend_slot = slot_num;
	}
}

void mem_write_config(uint8_t slot_num, int32_t pc_num){
	pend_slot = slot_num;
	pend_head = true;
	GO_Get_Config(&mem_buff);
	if (pc_num > 0) {
		pend_conf = true;
		if (!mem_handler->write_data((char*) &slot_pc[slot_num], 2, slot_num)) {
			pend_pc = true;
		}
	} else {
		if (!mem_handler->write_data((char*) &mem_buff, 3, slot_num)) {
			pend_conf = true;
		}
	}
}

int8_t mem_pc2slot(uint32_t pc_num){
	for (uint8_t i = 0; i < 128; i++){
		if (slot_pc[i] == pc_num){
			return i;
		}
	}
	return -1;
}

void mem_read_config_pc(uint32_t pc_num){
	int8_t slot_num = mem_pc2slot(pc_num);
	if (slot_num < 0) return;
	mem_read_config(slot_num);
}

void mem_update(){
	if (pend_slot >= 0){
		if (pend_pc){
			if (mem_handler->write_data((char*) &slot_pc[pend_slot], 2, pend_slot)) {
				pend_pc = false;
			}
		} else if (pend_conf){
			if (mem_handler->write_data((char*) &mem_buff, 3, pend_slot)) {
				pend_conf = false;
			}
		} else if (pend_read){
			if (mem_handler->read_data((char*) &mem_buff, 3, pend_slot)) {
				pend_read = false;
				is_reading = true;
			}
		} else if (pend_head) {
			head_buff[0] = pend_slot;
			if (mem_handler->write_data(head_buff, 0, 1)) {
				pend_head = false;
			}
		} else {
			pend_slot = -1;
		}
	}
	if (is_reading){
		is_reading = false;
		GO_Set_Config(&mem_buff);
	}
}
