/*
 * memory.h
 *
 * Created: 02/08/2024
 *  Author: GuavTek
 */


#ifndef MEMORY_H_
#define MEMORY_H_

#include <stdio.h>
#include "pico/stdlib.h"
#include "generic_output.h"
#include "eeprom_cat.h"

extern eeprom_cat_c* mem_handler;
extern const uint32_t MAX_MEM_SLOT;

// Define EEPROM layout
const eeprom_cat_conf_t EEPROM_CONF = {
	.maxAddr = RAM_SIZE-1
};

// TODO: finalize layout
// CC lookup table?
const eeprom_cat_section_t EEPROM_SECTIONS[2] = {
	{	// Header, 320 byte
		.offset = 0x0000,
		.objectSize = 8 // Contains 40 chunks
	},
	{	// Main
		.offset = 0x0140,
		.objectSize = sizeof(ConfigNVM_t)	// 354, 32kB-320B memory = 89 slots
	}
};

void mem_init();
void mem_read_config(uint8_t slot_num);
void mem_write_config(ConfigNVM_t* conf, uint8_t slot_num, int32_t pc_num);
int8_t mem_pc2slot(uint32_t pc_num);

#endif /* MEMORY_H_ */