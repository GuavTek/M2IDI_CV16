#ifndef _BOARD_M2IDI_USB_H
#define _BOARD_M2IDI_USB_H

// For board detection
#define BOARD_M2IDI_CV16

#ifndef PICO_XOSC_STARTUP_DELAY_MULTIPLIER
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64
#endif

// --- LED MATRIX ---
#ifndef LEDC1
#define LEDC1 14
#endif
#ifndef LEDC2
#define LEDC2 16
#endif
#ifndef LEDC3
#define LEDC3 17
#endif
#ifndef LEDC4
#define LEDC4 18
#endif
#ifndef LEDC5
#define LEDC5 20
#endif
#ifndef LEDC6
#define LEDC6 21
#endif
#ifndef LEDC7
#define LEDC7 22
#endif
#ifndef LEDC8
#define LEDC8 23
#endif
#ifndef LEDR1
#define LEDR1 29
#endif
#ifndef LEDR2
#define LEDR2 28
#endif
#ifndef LEDR3
#define LEDR3 27
#endif
#ifndef LEDR4
#define LEDR4 26
#endif
#ifndef LEDR5
#define LEDR5 19
#endif

// no PICO_DEFAULT_WS2812_PIN

// --- BUTTON ---
#ifndef BUTT1
#define BUTT1 2
#endif
#ifndef BUTT2
#define BUTT2 1
#endif
#ifndef BUTT3
#define BUTT3 0
#endif

// MUX pins
#ifndef M2IDI_MUXA_PIN
#define M2IDI_MUXA_PIN 8
#endif
#ifndef M2IDI_MUXB_PIN
#define M2IDI_MUXB_PIN 9
#endif
#ifndef M2IDI_MUXINH_PIN
#define M2IDI_MUXINH_PIN 10
#endif

// --- SPI ---
#ifndef M2IDI_SPI
#define M2IDI_SPI 0
#endif
#ifndef M2IDI_SPI_SCK_PIN
#define M2IDI_SPI_SCK_PIN 6
#endif
#ifndef M2IDI_SPI_TX_PIN
#define M2IDI_SPI_TX_PIN 7
#endif
#ifndef M2IDI_SPI_RX_PIN
#define M2IDI_SPI_RX_PIN 4
#endif
#ifndef M2IDI_SPI_CSN_CAN_PIN
#define M2IDI_SPI_CSN_CAN_PIN 25
#endif
#ifndef M2IDI_CAN_INT_PIN
#define M2IDI_CAN_INT_PIN 24
#endif
#ifndef M2IDI_SPI_CSN_EEPROM_PIN
#define M2IDI_SPI_CSN_EEPROM_PIN 3
#endif

#ifndef M2IDI_DAC_SPI_SCK_PIN
#define M2IDI_DAC_SPI_SCK_PIN 12
#endif
#ifndef M2IDI_DAC_SPI_TX_PIN
#define M2IDI_DAC_SPI_TX_PIN 11
#endif
#ifndef M2IDI_DAC_SPI_CSN_PIN
#define M2IDI_DAC_SPI_CSN_PIN 13
#endif

// --- RAM ---
#define RAM_SIZE 32000 // in bytes

// --- FLASH ---

#define PICO_BOOT_STAGE2_CHOOSE_IS25LP080 1
#define PICO_BOOT_STAGE2_CHOOSE_GENERIC_03H 0

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

#endif
