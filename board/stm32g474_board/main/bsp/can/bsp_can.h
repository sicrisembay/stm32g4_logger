/*
 * bsp_can.h
 *
 *  Created on: Sep 8, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef BSP_CAN_H_
#define BSP_CAN_H_

#include "logger_conf.h"
#include "stm32g4xx_hal_fdcan.h"
#include "stdbool.h"

#define CONFIG_CANFD_DATA_SIZE      (64)

typedef enum {
#if (CONFIG_CAN_COUNT >= 1)
    CAN_ONE = 0,
#endif
#if (CONFIG_CAN_COUNT >= 2)
    CAN_TWO,
#endif
#if (CONFIG_CAN_COUNT >= 3)
    CAN_TRHEE,
#endif
    N_CAN_ID
} CAN_ID_T;


typedef enum {
    ARBIT_500KBPS = 0,
    ARBIT_1MBPS,
    N_ARBIT_BITRATE
} ARBIT_BITRATE_T;


typedef enum {
    DATA_500KBPS = 0,
    DATA_1MBPS,
    DATA_2MBPS,
    N_DATA_BITRATE
} DATA_BITRATE_T;


typedef struct {
    FDCAN_TxHeaderTypeDef header;
    uint8_t data[CONFIG_CANFD_DATA_SIZE];
} CAN_TX_T;

typedef struct {
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[CONFIG_CANFD_DATA_SIZE];
} CAN_RX_T;

void BSP_CAN_init(void);
bool BSP_CAN_configure(const CAN_ID_T id,
                       const ARBIT_BITRATE_T arbit_bps,
                       const DATA_BITRATE_T data_bps);
bool BSP_CAN_start(const CAN_ID_T id);
bool BSP_CAN_stop(const CAN_ID_T id);
bool BSP_CAN_send(const CAN_ID_T id, CAN_TX_T * pElem);

#endif /* BSP_CAN_H_ */
