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
const eeprom_cat_section_t EEPROM_SECTIONS[4] = {
	{	// Header, 256 byte
		.offset = 0x0000,
		.objectSize = 4 // Contains 32 items
	},
	{	// CC lookup, 1024 byte
		.offset = 0x0100,
		.objectSize = sizeof(ctrlSource_t)	// 256 items
	},
	{	// PC for saveslots, 512 byte
		.offset = 0x0100 + 256*sizeof(ctrlSource_t),
		.objectSize = 4	// 128 items
	},
	{	// Main
		.offset = 0x0300 + 256*sizeof(ctrlSource_t),
		.objectSize = sizeof(ConfigNVM_t)	// 354 byte
	}
};

void mem_init();
void mem_update();
void mem_read_config(uint8_t slot_num);
void mem_read_config_pc(uint32_t pc_num);
void mem_write_config(uint8_t slot_num, int32_t pc_num);

#endif /* MEMORY_H_ */