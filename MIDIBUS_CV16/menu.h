/*
 * menu.h
 *
 * Created: 19/07/2021 17:01:16
 *  Author: GuavTek
 */ 


#ifndef MENU_H_
#define MENU_H_

struct menuNode {
	const uint8_t graphic[5];
	menuNode* const kid;
	menuNode* const previous;
	menuNode* const next;
	void const (*function)();
};

void Menu_Init();
void Menu_Service();



#endif /* MENU_H_ */