/*
 * LEDMatrix.h
 *
 * Created: 17/07/2021 18:31:55
 *  Author: GuavTek
 */ 


#ifndef LEDMATRIX_H_
#define LEDMATRIX_H_

#define LEDR1_PORT 0
#define LEDR2_PORT 0
#define LEDR3_PORT 0
#define LEDR4_PORT 0
#define LEDR5_PORT 1
#define LEDC1_PORT 0
#define LEDC2_PORT 0
#define LEDC3_PORT 0
#define LEDC4_PORT 0
#define LEDC5_PORT 1
#define LEDC6_PORT 0
#define LEDC7_PORT 0
#define LEDC8_PORT 0

#define LEDR1_PIN 23
#define LEDR2_PIN 22
#define LEDR3_PIN 21
#define LEDR4_PIN 20
#define LEDR5_PIN 10
#define LEDC1_PIN 8
#define LEDC2_PIN 9
#define LEDC3_PIN 10
#define LEDC4_PIN 11
#define LEDC5_PIN 11
#define LEDC6_PIN 17
#define LEDC7_PIN 18
#define LEDC8_PIN 19

extern uint8_t lmData[5];

void LM_Init();
void LM_Service();

inline uint8_t LM_ReadRow(uint8_t row){
	return lmData[row];
}

inline void LM_WriteRow(uint8_t row, uint8_t data){
	lmData[row] = data;
}



#endif /* LEDMATRIX_H_ */