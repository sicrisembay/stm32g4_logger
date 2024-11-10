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


/*!
 * <PRE>void LPUART_Init(void);</PRE>
 *
 * This function initializes the UART manager and its underlying peripherals.
 *
 * \param None
 *
 * \return void
 */
void LPUART_Init(void);


/*!
 * <PRE>int32_t LPUART_Send(const uint8_t * buf, const uint32_t len);</PRE>
 *
 * This function sends buf to UART manager.
 *
 * \param buf   Pointer to buffer
 * \param len   Number of bytes to send
 *
 * \return Number of bytes sent
 */
int32_t LPUART_Send(const uint8_t * buf, const uint32_t len);


/*!
 * <PRE>bool LPUART_TxDone(void);</PRE>
 *
 * This function returns true if the UART manager transmission is done.
 *
 * \param None
 *
 * \return true: transmission done, false: otherwise
 */
bool LPUART_TxDone(void);


/*!
 * <PRE>int32_t LPUART_Receive(uint8_t * buf, const uint32_t len);</PRE>
 *
 * This function receives data from UART manager.
 *
 * \param buf   Pointer to buffer
 * \param len   Number of bytes to receive
 *
 * \return Actual number of bytes received
 */
int32_t LPUART_Receive(uint8_t * buf, const uint32_t len);

int32_t LPUART_printf(const char * format, ...);

#endif /* BSP_LPUART_H_ */
