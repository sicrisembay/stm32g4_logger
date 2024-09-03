/*
 * test_board.c
 *
 *  Created on: Sep 2, 2024
 *      Author: Sicris Rey Embay
 */

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"

static bool bInit = false;

static BaseType_t CmdReset(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    NVIC_SystemReset();
    return 0;
}


static const CLI_Command_Definition_t board_reset = {
    "reset",
    "reset:\r\n"
    "\tReset MCU\r\n\r\n",
    CmdReset,
    0
};


void TEST_BOARD_Init(void)
{
    if(bInit) {
        return;
    }

    FreeRTOS_CLIRegisterCommand(&board_reset);
    bInit = true;
}


