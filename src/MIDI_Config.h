/*
 * MIDI_Config.h
 *
 * Created: 15/10/2021 22:31:05
 *  Author: GuavTek
 */

// Configurations for MIDI application

#ifndef MIDI_CONFIG_H_
#define MIDI_CONFIG_H_

#include "SPI_RP2040.h"
#include "dac_spi.h"
#include "MCP2517.h"
#include "eeprom_cat.h"

// Define CAN filters
const CAN_Filter_t CAN_FLT0 = {
	.enabled = true,
	.fifoDestination = 1,
	.extendedID = false,
	.ID = (0 << 10),
	.matchBothIDTypes = false,
	.maskID = 1 << 10
};

const CAN_Filter_t CAN_FLT1 = {
	.enabled = true,
	.fifoDestination = 1,
	.extendedID = false,
	.ID = (1 << 10) | 420,
	.matchBothIDTypes = false,
	.maskID = 0x7ff
};

// Define FIFO configurations
// Rx FIFO
const CAN_FIFO_t CAN_FIFO1 = {
	.enabled = true,
	.payloadSize = 64,
	.fifoDepth = 18,
	.retransmitAttempt = CAN_FIFO_t::unlimited,
	.messagePriority = 0,
	.txEnable = false,
	.autoRemote = false,
	.receiveTimestamp = false,
	.exhaustedTxInterrupt = false,
	.overflowInterrupt = false,
	.fullEmptyInterrupt = false,
	.halfFullEmptyInterrupt = false,
	.notFullEmptyInterrupt = true
};

// Tx FIFO
const CAN_FIFO_t CAN_FIFO2 = {
	.enabled = true,
	.payloadSize = 64,
	.fifoDepth = 8,
	.retransmitAttempt = CAN_FIFO_t::unlimited,
	.messagePriority = 0,
	.txEnable = true,
	.autoRemote = false,
	.receiveTimestamp = false,
	.exhaustedTxInterrupt = false,
	.overflowInterrupt = false,
	.fullEmptyInterrupt = false,
	.halfFullEmptyInterrupt = false,
	.notFullEmptyInterrupt = false
};


const dac_spi_config_t SPI_DAC_CONF = {
	.dma_irq_num = 0,
	.polarity = 0,
	.speed = 30000000,
	.pin_tx = M2IDI_DAC_SPI_TX_PIN,
	.pin_ck = M2IDI_DAC_SPI_SCK_PIN,
	.pin_cs = M2IDI_DAC_SPI_CSN_PIN
};

const spi_config_t SPI_CAN_CONF = {
	.dma_irq_num = 1,
	.phase = SPI_CPHA_0,
	.polarity = SPI_CPOL_0,
	.order = SPI_MSB_FIRST,
	.speed = 16000000,
	.pin_tx = M2IDI_SPI_TX_PIN,
	.pin_rx = M2IDI_SPI_RX_PIN,
	.pin_ck = M2IDI_SPI_SCK_PIN,
	.pin_cs = {M2IDI_SPI_CSN_CAN_PIN, M2IDI_SPI_CSN_EEPROM_PIN}
};

const CAN_Config_t CAN_CONF = {
	.clkOutDiv = CAN_Config_t::clkOutDiv1,
	.sysClkDiv = false,
	.clkDisable = false,
	.pllEnable = false,
	.txBandwidthShare = CAN_Config_t::BW_Share4,
	.opMode = CAN_MODE_E::Normal_FD,
	.txQueueEnable = false,
	.txEventStore = false,
	.listenOnlyOnError = false,
	.restrictRetransmit = false,
	.disableBitrateSwitch = false,
	.wakeFilter = CAN_Config_t::Wake_Filter_40_75ns,
	.enableWakeFilter = false,
	.disableProtocolException = false,
	.enableIsoCrc = true,
	.deviceNetFilterBits = 0,
	.nominalBaudPrescaler = 1,	// Time quanta prescaler Tq = 2/20MHz = 100ns
	.nominalTSEG1 = 6,			// Time segment 1 = 7 Tq
	.nominalTSEG2 = 1,			// Time segment 2 = 2 Tq
	.nominalSyncJump = 0,		// Sync jump width = 1 Tq
	.dataBaudPrescaler = 0,		// Time quanta Tq = 1/20MHz = 50ns
	.dataTSEG1 = 1,				// 2 Tq
	.dataTSEG2 = 1,				// 2 Tq
	.dataSyncJump = 0,			// 1 Tq
	.enableEdgeFilter = true,
	.enableSID11 = false,
	.txDelayCompensation = CAN_Config_t::TdcAuto,
	.txDelayCompOffset = 0,
	.enableIntInvalidMessage = false,
	.enableIntWakeup = false,
	.enableIntBusError = false,
	.enableIntSysError = false,
	.enableIntRxOverflow = false,
	.enableIntTxAttempt = false,
	.enableIntCrcError = false,
	.enableIntEccError = false,
	.enableIntTxEvent = false,
	.enableIntModeChange = false,
	.enableIntTimebaseCount = false,
	.enableIntRxFifo = true,
	.enableIntTxFifo = false
};


#endif /* MIDI_CONFIG_H_ */