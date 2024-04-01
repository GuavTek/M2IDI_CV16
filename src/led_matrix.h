/*
 * LEDMatrix.h
 *
 * Created: 17/07/2021 18:31:55
 *  Author: GuavTek
 */ 


#ifndef LED_MATRIX_H_
#define LED_MATRIX_H_

extern uint16_t lmData[5];

void LM_Init();
void LM_Service();

inline uint16_t LM_ReadRow(uint8_t row){
	return lmData[row];
}

inline void LM_WriteRow(uint8_t row, uint16_t data){
	lmData[row] = data;
}



#endif /* LED_MATRIX_H_ */