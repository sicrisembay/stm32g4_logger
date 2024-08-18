/*
 * lpuart.h
 *
 *  Created on: Aug 17, 2024
 *      Author: Sicris
 */

#ifndef BSP_LPUART_H_
#define BSP_LPUART_H_

#include "stdbool.h"
#include "stdint.h"

#define LPUART_ERR_INVALID_ARG          (-1)
#define LPUART_ERR_INVALID_STATE        (-2)
#define LPUART_ERR_MUTEX                (-3)
#define LPUART_ERR_SPACE_INSUFFICIENT   (-4)

typedef enum {
    UART_TX_STATE_IDLE = 0,
    UART_TX_STATE_BUSY
} uart_tx_state_t;

void LPUART_Init(void);
int32_t LPUART_Send(uint8_t * buf, const uint32_t len);
bool LPUART_TxDone(void);
int32_t LPUART_Receive(uint8_t * buf, const uint32_t len);

#endif /* BSP_LPUART_H_ */
