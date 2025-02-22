/*
 * test_usb_msc.c
 *
 *  Created on: Feb 21, 2025
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
#include "test_usb_msc.h"
#include "usb_msc.h"

static bool bInit = false;

static BaseType_t FuncMscMount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);
    usb_msc_mount();

    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");

    return 0;
}

static const CLI_Command_Definition_t msc_mount = {
    "msc_mount",
    "msc_mount\r\n"
    "\tMakes the drive ready and mountable.\r\n\r\n",
    FuncMscMount,
    0
};


static BaseType_t FuncMscUnMount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);
    usb_msc_unmount();

    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");

    return 0;
}

static const CLI_Command_Definition_t msc_umount = {
    "msc_umount",
    "msc_umount\r\n"
    "\tMakes the drive unavailable.\r\n\r\n",
    FuncMscUnMount,
    0
};


void TEST_USB_MSC_Init(void)
{
    if(bInit) {
        return;
    }

    FreeRTOS_CLIRegisterCommand(&msc_mount);
    FreeRTOS_CLIRegisterCommand(&msc_umount);

    bInit = true;
}
