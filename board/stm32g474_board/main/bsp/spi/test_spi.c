/*
 * test_spi.c
 *
 *  Created on: Aug 20, 2024
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
#include "test_spi.h"
#include "bsp_spi.h"

#define TEST_SPI_BUFSZ      (64)

static bool bInit = false;
static uint8_t spiBuffer[TEST_SPI_BUFSZ] = {0};
static SemaphoreHandle_t semHandle = NULL;
static StaticSemaphore_t semStruct;

static BaseType_t CmdSpiTransact(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    static uint32_t internalState = 0;
    static uint32_t printIdx = 0;
    static int32_t nBytes = 0;
    int32_t i32Temp;
    char * ptrStrParam;
    char tmpStr[12];
    BaseType_t strParamLen;
    char * ptrEnd;
    SPI_MODE_T mode;
    int32_t status;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(internalState == 0) {
        /*
         * SPI Mode
         */
        ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString,
                                    1, &strParamLen);
        if(ptrStrParam == NULL) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 1 not found!\r\n\r\n");
            return 0;
        }
        if(strParamLen > (sizeof(tmpStr) - 1)) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter len exceeded buffer!\r\n\r\n");
            return 0;
        }
        memcpy(tmpStr, ptrStrParam, strParamLen);
        tmpStr[strParamLen] = '\0';
        errno = 0;
        i32Temp = strtol(tmpStr, &ptrEnd, 0);
        if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
           (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 1 value is invalid!\r\n\r\n");
            return 0;
        }
        if((i32Temp < 0) || (i32Temp > N_SPI_MODE)) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Invalid mode!\r\n\r\n");
            return 0;
        }
        mode = (SPI_MODE_T)i32Temp;
        /*
         * Data
         */
        for(nBytes = 0; nBytes < TEST_SPI_BUFSZ; nBytes++) {
            /* Get data */
            ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString,
                    2 + nBytes, &strParamLen);
            if(ptrStrParam == NULL) {
                break;
            }
            memcpy(tmpStr, ptrStrParam, strParamLen);
            tmpStr[strParamLen] = '\0';
            errno = 0;
            i32Temp = strtol(tmpStr, &ptrEnd, 0);
            if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
               (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
                break;
            }
            spiBuffer[nBytes] = (uint8_t)i32Temp;
        }

        while(pdTRUE == xSemaphoreTake(semHandle, 0));
        int32_t spiRet = BSP_SPI_transact(spiBuffer, spiBuffer, nBytes, mode, NULL, semHandle, &status);
        if(SPI_ERR_NONE != spiRet) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\tSPI transact return %ld\r\n\r\n", spiRet);
            return 0;
        }
        if(pdTRUE != xSemaphoreTake(semHandle, 100)) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\tSPI transact timeout\r\n\r\n");
            return 0;
        }
        if(status < 0) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\tSPI transact failed %ld\r\n\r\n", status);
            return 0;
        }

        printIdx = 0;
        internalState++;
        return 1;
    } else if(internalState == 1) {
        /* Display SPI MISO values */
        snprintf(pcWriteBuffer, xWriteBufferLen, "\t");
        while(nBytes > 0) {
            snprintf(tmpStr, 8, "%02x ", spiBuffer[printIdx]);
            strcat(pcWriteBuffer, tmpStr);
            nBytes--;
            printIdx++;
            if((printIdx % 8) == 0) {
                strcat(pcWriteBuffer, "\r\n");
                return 1;
            }
        }
        internalState++;
        return 1;
    } else {
        printIdx = 0;
        internalState = 0;
        snprintf(pcWriteBuffer, xWriteBufferLen, "\r\n\r\n");
        return 0;
    }
}

static const CLI_Command_Definition_t spi_transact = {
    "spi_transact",
    "spi_transact <mode> <data>:\r\n"
    "    Send <data> to SPI MOSI and prints MISO data\r\n\r\n",
    CmdSpiTransact,
    -1
};

void TEST_SPI_init(void)
{
    if(bInit != true) {
        memset(spiBuffer, 0, sizeof(spiBuffer));

        semHandle = xSemaphoreCreateBinaryStatic(&semStruct);
        configASSERT(NULL != semHandle);

        FreeRTOS_CLIRegisterCommand(&spi_transact);

        bInit = true;
    }
}


