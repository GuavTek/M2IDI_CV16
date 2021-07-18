/*
 * PWM.h
 *
 * Created: 18/07/2021 16:37:38
 *  Author: GuavTek
 */ 


#ifndef PWM_H_
#define PWM_H_

void PWM_Init();
void PWM_Set(uint8_t row, uint8_t col, uint16_t data);
void PWM_Service();

#endif /* PWM_H_ */