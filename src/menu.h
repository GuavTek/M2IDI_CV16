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

typedef void (*void_function)();
struct menuNode {
	const uint8_t graphic[5];
	menuNode* const kid;
	menuNode* const previous;
	menuNode* const next;
	const void_function function;
};

enum menu_status_t {
	Navigate,
	SetLFO,
	Edit_8bit,
	Edit_16bit,
	Edit_32bit,
	Edit_int,
	Wait_MIDI
};

void menu_init();
uint8_t menu_service();
uint8_t menu_midi(struct umpCVM* msg);

menu_status_t get_menu_state();


#endif /* MENU_H_ */