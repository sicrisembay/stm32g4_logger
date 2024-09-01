/*
 * sdcard.h
 *
 *  Created on: Aug 31, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef BSP_SDCARD_SDCARD_H_
#define BSP_SDCARD_SDCARD_H_

#include "stdint.h"
#include "bsp/spi/bsp_spi.h"

#define SDCARD_ERR_NONE                 (0)
// Don't overlap with SPI error values
#define SDCARD_ERR_SCHED_NOT_STARTED    (SPI_ERR_LASTENTRY)
#define SDCARD_ERR_NOT_PRESENT          (SPI_ERR_LASTENTRY-1)
#define SDCARD_ERR_TIMEOUT              (SPI_ERR_LASTENTRY-2)
#define SDCARD_ERR_UNKNOWN_CARD         (SPI_ERR_LASTENTRY-3)
#define SDCARD_ERR_UNSUPPORTED          (SPI_ERR_LASTENTRY-4)
#define SDCARD_ERR_R1                   (SPI_ERR_LASTENTRY-5)

int32_t SDCARD_Init(void);
int32_t SDCARD_GetBlocksNumber(uint32_t * num);
int32_t SDCARD_ReadSingleBlock(uint32_t blockNum, uint8_t * buff); // sizeof(buff) == 512!
int32_t SDCARD_WriteSingleBlock(uint32_t blockNum, const uint8_t * buff); // sizeof(buff) == 512!

// Read Multiple Blocks
int32_t SDCARD_ReadBegin(uint32_t blockNum);
int32_t SDCARD_ReadData(uint8_t * buff); // sizeof(buff) == 512!
int32_t SDCARD_ReadEnd();

// Write Multiple Blocks
int32_t SDCARD_WriteBegin(uint32_t blockNum);
int32_t SDCARD_WriteData(const uint8_t * buff); // sizeof(buff) == 512!
int32_t SDCARD_WriteEnd();

// TODO: read lock flag? CMD13, SEND_STATUS

#endif /* BSP_SDCARD_SDCARD_H_ */
