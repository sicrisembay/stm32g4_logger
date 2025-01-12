/*
 * test_can.c
 *
 *  Created on: Sep 18, 2024
 *      Author: Sicris Rey Embay
 */

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"
#include "test_can.h"
#include "bsp_can.h"

#define TAG_TEST_CAN   "cli_can"

static bool bInit = false;
static CAN_TX_T canTxElem;

static BaseType_t CmdCanStart(
        char *pcWriteBuffer,
        size_t xWriteBufferLen,
        const char *pcCommandString)
{
    char * ptrStrParam;
    char tmpStr[12];
    BaseType_t strParamLen;
    char * ptrEnd;
    int32_t i32Temp;
    CAN_ID_T id;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString,
                                1,
                                &strParamLen);
    if(ptrStrParam == NULL) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 1 not found!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if(strParamLen > (sizeof(tmpStr) - 1)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter len exceeded buffer!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    memcpy(tmpStr, ptrStrParam, strParamLen);
    tmpStr[strParamLen] = '\0';
    errno = 0;
    i32Temp = strtol(tmpStr, &ptrEnd, 0);
    if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
       (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 1 value is invalid!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if((i32Temp < 0) || (i32Temp >= N_CAN_ID)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Invalid CAN peripheral\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    id = (CAN_ID_T)i32Temp;

    BSP_CAN_start(id);
    snprintf(pcWriteBuffer, xWriteBufferLen,
            "I (%ld) " TAG_TEST_CAN
            ": OK\r\n\r\n", xTaskGetTickCount());
    return 0;
}


static const CLI_Command_Definition_t can_start = {
    "can_start",
    "can_start <id>:\r\n"
    "\tEnable CAN<id>\r\n\r\n",
    CmdCanStart,
    1
};


static BaseType_t CmdCanStop(
        char *pcWriteBuffer,
        size_t xWriteBufferLen,
        const char *pcCommandString)
{
    char * ptrStrParam;
    char tmpStr[12];
    BaseType_t strParamLen;
    char * ptrEnd;
    int32_t i32Temp;
    CAN_ID_T id;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString,
                                1,
                                &strParamLen);
    if(ptrStrParam == NULL) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 1 not found!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if(strParamLen > (sizeof(tmpStr) - 1)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter len exceeded buffer!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    memcpy(tmpStr, ptrStrParam, strParamLen);
    tmpStr[strParamLen] = '\0';
    errno = 0;
    i32Temp = strtol(tmpStr, &ptrEnd, 0);
    if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
       (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 1 value is invalid!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if((i32Temp < 0) || (i32Temp >= N_CAN_ID)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Invalid CAN peripheral!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    id = (CAN_ID_T)i32Temp;

    BSP_CAN_stop(id);
    snprintf(pcWriteBuffer, xWriteBufferLen,
            "I (%ld) " TAG_TEST_CAN
            ": OK\r\n\r\n", xTaskGetTickCount());
    return 0;
}


static const CLI_Command_Definition_t can_stop = {
    "can_stop",
    "can_stop <id>:\r\n"
    "\tDisable CAN<id>\r\n\r\n",
    CmdCanStop,
    1
};


static BaseType_t CmdCanSend(
        char *pcWriteBuffer,
        size_t xWriteBufferLen,
        const char *pcCommandString)
{
    char * ptrStrParam;
    char tmpStr[12];
    BaseType_t strParamLen;
    char * ptrEnd;
    int32_t i32Temp;
    CAN_ID_T periph;
    uint32_t msgId;
    uint32_t dataLength;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /*
     * Parameter1: Peripheral ID
     */
    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString,
                                1,
                                &strParamLen);
    if(ptrStrParam == NULL) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 1 not found!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if(strParamLen > (sizeof(tmpStr) - 1)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 1 len exceeded buffer!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    memcpy(tmpStr, ptrStrParam, strParamLen);
    tmpStr[strParamLen] = '\0';
    errno = 0;
    i32Temp = strtol(tmpStr, &ptrEnd, 0);
    if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
       (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 1 value is invalid!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if((i32Temp < 0) || (i32Temp >= N_CAN_ID)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Invalid CAN peripheral!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    periph = (CAN_ID_T)i32Temp;

    /*
     * Parameter2: CAN Message ID
     */
    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString,
                                2,
                                &strParamLen);
    if(ptrStrParam == NULL) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 2 not found!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if(strParamLen > (sizeof(tmpStr) - 1)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 2 len exceeded buffer!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    memcpy(tmpStr, ptrStrParam, strParamLen);
    tmpStr[strParamLen] = '\0';
    errno = 0;
    i32Temp = strtol(tmpStr, &ptrEnd, 0);
    if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
       (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Parameter 2 value is invalid!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    if((i32Temp < 0) || (i32Temp >= 0x7FF)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": Invalid CAN 11-bit message ID!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }
    msgId = (uint32_t)i32Temp;

    /*
     * Parameter3: CAN data
     */
    memset(canTxElem.data, 0, CONFIG_CANFD_DATA_SIZE);
    for(dataLength = 0; dataLength < CONFIG_CANFD_DATA_SIZE; dataLength++) {
        ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString,
                            3 + dataLength, &strParamLen);
        if(ptrStrParam == NULL) {
            break;
        }
        if(strParamLen > (sizeof(tmpStr) - 1)) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "E (%ld) " TAG_TEST_CAN
                    ": Parameter 3 len exceeded buffer!\r\n\r\n", xTaskGetTickCount());
            return 0;
        }
        memcpy(tmpStr, ptrStrParam, strParamLen);
        tmpStr[strParamLen] = '\0';
        errno = 0;
        i32Temp = strtol(tmpStr, &ptrEnd, 0);
        if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
           (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "E (%ld) " TAG_TEST_CAN
                    ": Parameter 3 value is invalid!\r\n\r\n", xTaskGetTickCount());
            return 0;
        }
        canTxElem.data[dataLength] = (uint8_t)i32Temp;
    }

    canTxElem.header.Identifier = msgId;
    canTxElem.header.IdType = FDCAN_STANDARD_ID;
    canTxElem.header.TxFrameType = FDCAN_DATA_FRAME;
    canTxElem.header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    canTxElem.header.BitRateSwitch = FDCAN_BRS_OFF;
    canTxElem.header.FDFormat = FDCAN_CLASSIC_CAN;
    canTxElem.header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    canTxElem.header.MessageMarker = 0;
    if(dataLength <= 8) {
        canTxElem.header.DataLength = dataLength;
    } else if(dataLength <= 12) {
        canTxElem.header.DataLength = FDCAN_DLC_BYTES_12;
    } else if(dataLength <= 16) {
        canTxElem.header.DataLength = FDCAN_DLC_BYTES_16;
    } else if(dataLength <= 20) {
        canTxElem.header.DataLength = FDCAN_DLC_BYTES_20;
    } else if(dataLength <= 24) {
        canTxElem.header.DataLength = FDCAN_DLC_BYTES_24;
    } else if(dataLength <= 32) {
        canTxElem.header.DataLength = FDCAN_DLC_BYTES_32;
    } else {
        canTxElem.header.DataLength = FDCAN_DLC_BYTES_64;
    }

    if(!BSP_CAN_is_enabled(periph)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": CAN%d not enabled!\r\n\r\n", xTaskGetTickCount(), (periph + 1));
        return 0;
    }

    if(!BSP_CAN_send(periph, &canTxElem)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "E (%ld) " TAG_TEST_CAN
                ": BSP_CAN_send Failed!\r\n\r\n", xTaskGetTickCount());
        return 0;
    }

    snprintf(pcWriteBuffer, xWriteBufferLen,
            "I (%ld) " TAG_TEST_CAN
            ": OK\r\n\r\n", xTaskGetTickCount());
    return 0;
}


static const CLI_Command_Definition_t can_send = {
    "can_send",
    "can_send <periph> <id> <data>:\r\n"
    "\tSend <data> to <periph>\r\n\r\n",
    CmdCanSend,
    -1
};


void TEST_CAN_init(void)
{
    if(bInit != true) {
        FreeRTOS_CLIRegisterCommand(&can_start);
        FreeRTOS_CLIRegisterCommand(&can_stop);
        FreeRTOS_CLIRegisterCommand(&can_send);

        bInit = true;
    }
}
