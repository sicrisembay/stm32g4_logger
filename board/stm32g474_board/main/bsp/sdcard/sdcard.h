/*
 * sdcard.h
 *
 *  Created on: Aug 31, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef BSP_SDCARD_SDCARD_H_
#define BSP_SDCARD_SDCARD_H_

#include "logger_conf.h"

#if CONFIG_USE_SDCARD

#include "stdint.h"
#include "stdbool.h"
#include "bsp/spi/bsp_spi.h"

#define SDCARD_ERR_NONE                 (0)
// Don't overlap with SPI error values
#define SDCARD_ERR_SCHED_NOT_STARTED    (SPI_ERR_LASTENTRY)
#define SDCARD_ERR_INVALID_ARG          (SPI_ERR_LASTENTRY-1)
#define SDCARD_ERR_NOT_PRESENT          (SPI_ERR_LASTENTRY-2)
#define SDCARD_ERR_TIMEOUT              (SPI_ERR_LASTENTRY-3)
#define SDCARD_ERR_UNKNOWN_CARD         (SPI_ERR_LASTENTRY-4)
#define SDCARD_ERR_UNSUPPORTED          (SPI_ERR_LASTENTRY-5)
#define SDCARD_ERR_R1                   (SPI_ERR_LASTENTRY-6)
#define SDCARD_ERR_WAIT_DATA_TOKEN      (SPI_ERR_LASTENTRY-7)
#define SDCARD_ERR_NOT_INITIALIZED      (SPI_ERR_LASTENTRY-8)
#define SDCARD_ERR_WRITE_REJECTED       (SPI_ERR_LASTENTRY-9)

#define SDCARD_BLOCK_SIZE               (512)   // READ_BL_LEN or
                                                // WRITE_BL_LEN

#define SDCARD_SECTOR_SIZE              (65536) // 64kB
#define SDCARD_CID_DATA_SIZE            (16)
#define SDCARD_CSD_DATA_SIZE            (16)

int32_t SDCARD_Init(void);
bool SDCARD_InitDone(void);
bool SDCARD_ready(void);
int32_t SDCARD_GetBlocksNumber(uint32_t * num);
int32_t SDCARD_ReadOCR(uint32_t * pOCR);
int32_t SDCARD_ReadCardIdentification(uint8_t * buff, size_t buffLen);
int32_t SDCARD_ReadCardSpecificData(uint8_t * buff, size_t buffLen);
int32_t SDCARD_ReadSingleBlock(uint32_t blockNum, uint8_t * buff, size_t buffLen);
int32_t SDCARD_WriteSingleBlock(uint32_t blockNum, const uint8_t * buff, size_t buffLen);

// Read Multiple Blocks
int32_t SDCARD_ReadBegin(uint32_t blockNum);
int32_t SDCARD_ReadData(uint8_t * buff); // sizeof(buff) == 512!
int32_t SDCARD_ReadEnd();

// Write Multiple Blocks
int32_t SDCARD_WriteBegin(uint32_t blockNum);
int32_t SDCARD_WriteData(const uint8_t * buff); // sizeof(buff) == 512!
int32_t SDCARD_WriteEnd();

uint32_t SDCARD_GetBlockCount(void);

// TODO: read lock flag? CMD13, SEND_STATUS

#endif /* CONFIG_USE_SDCARD */
#endif /* BSP_SDCARD_SDCARD_H_ */
