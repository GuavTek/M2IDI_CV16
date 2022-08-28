/*
 * menu.h
 *
 * Created: 19/07/2021 17:01:16
 *  Author: GuavTek
 */ 


#ifndef MENU_H_
#define MENU_H_

#include "MIDI_Driver.h"

struct menuNode {
	const uint8_t graphic[5];
	menuNode* const kid;
	menuNode* const previous;
	menuNode* const next;
	void const (*function)();
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

void Menu_Init();
uint8_t Menu_Service();
uint8_t Menu_MIDI(MIDI2_voice_t* msg);

menu_status_t Get_Menu_State();


#endif /* MENU_H_ */