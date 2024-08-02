#include "memory.h"

const uint32_t MAX_MEM_SLOT = ((RAM_SIZE-320)/sizeof(ConfigNVM_t));

eeprom_cat_c* mem_handler;
void eeprom_cb();

void mem_init(){
	mem_handler->init(EEPROM_CONF, EEPROM_SECTIONS, 2);
	mem_handler->set_callback(eeprom_cb);
}

void mem_read_config(uint8_t slot_num){

}

void mem_write_config(ConfigNVM_t* conf, uint8_t slot_num, int32_t pc_num){

}

int8_t mem_pc2slot(uint32_t pc_num){

}

void eeprom_cb(){

}
