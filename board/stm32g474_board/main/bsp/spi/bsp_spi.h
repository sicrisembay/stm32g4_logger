/*
 * bsp_spi.h
 *
 *  Created on: Aug 20, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef BSP_BSP_SPI_H_
#define BSP_BSP_SPI_H_

#include "logger_conf.h"
#include "stdbool.h"
#include "FreeRTOS.h"
#include "semphr.h"

#if (CONFIG_APB2_PERIPH_FREQ == 40000000)
typedef enum {
    BSP_SPI_CLK_20MHZ = 0,
    BSP_SPI_CLK_10MHZ,
    BSP_SPI_CLK_5MHZ,
    BSP_SPI_CLK_2500KHZ,
    BSP_SPI_CLK_1250KHZ,
    BSP_SPI_CLK_625KHZ,
    BSP_SPI_CLK_312KHZ,
    BSP_SPI_CLK_156KHZ,
    N_BSP_SPI_CLK
} BSP_SPI_CLK_T;
#else
#error "APB2 must be 40MHz!"
#endif

#define SPI_ERR_NONE                    (0)
#define SPI_ERR_INVALID_ARG             (-1)
#define SPI_ERR_INVALID_STATE           (-2)
#define SPI_ERR_Q_FULL                  (-3)
#define SPI_ERR_TIMEOUT                 (-4)
#define SPI_ERR_DMA_TRANSFER_ERROR      (-5)
#define SPI_ERR_LASTENTRY               (-6)

typedef void (* SPI_ChipSelect)(bool bSelect);

typedef enum {
    SPI_MODE0 = 0,
    SPI_MODE1,
    SPI_MODE2,
    SPI_MODE3,
    N_SPI_MODE
} SPI_MODE_T;


void BSP_SPI_init(void);
int32_t BSP_SPI_transact(void * pTxBuf,
                      void * pRxBuf,
                      size_t length,
                      SPI_MODE_T mode,
                      SPI_ChipSelect cs,
                      BSP_SPI_CLK_T clk,
                      SemaphoreHandle_t semRequester,
                      int32_t * pStatus);


#endif /* BSP_BSP_SPI_H_ */
