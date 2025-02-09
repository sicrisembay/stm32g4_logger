/*
 * cli.h
 *
 *  Created on: Aug 18, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef FREERTOS_PLUS_CLI_CLI_H_
#define FREERTOS_PLUS_CLI_CLI_H_

#define CLI_ERR_INVALID_ARG          (-1)
#define CLI_ERR_INVALID_STATE        (-2)
#define CLI_ERR_MUTEX                (-3)
#define CLI_ERR_SPACE_INSUFFICIENT   (-4)

void CLI_init(void);
void CLI_Receive(uint8_t* pBuf, uint32_t len);
int32_t CLI_printf(const char * format, ...);

#endif /* FREERTOS_PLUS_CLI_CLI_H_ */
