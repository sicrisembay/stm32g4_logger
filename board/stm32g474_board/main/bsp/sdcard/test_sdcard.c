/*
 * test_sdcard.c
 *
 *  Created on: Sep 1, 2024
 *      Author: Sicris Rey Embay
 */

#include "logger_conf.h"

#if CONFIG_USE_SDCARD

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"
#include "sdcard.h"
#include "test_sdcard.h"

static bool bInit = false;
static uint8_t blockData[SDCARD_BLOCK_SIZE];

static BaseType_t CmdSdCardInit(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    int32_t ret = SDCARD_ERR_NONE;
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    ret = SDCARD_Init();
    if(ret != SDCARD_ERR_NONE) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tInit Error %ld\r\n\r\n", ret);
        return 0;
    }

    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    return 0;
}


static const CLI_Command_Definition_t sdcard_init = {
    "sd_init",
    "sd_init:\r\n"
    "\tInitialize SD Card\r\n\r\n",
    CmdSdCardInit,
    0
};


static BaseType_t CmdSdCardCid(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    static uint32_t internalState = 0;
    static uint32_t printIdx = 0;
    char tmpStr[16];
    int32_t ret = SDCARD_ERR_NONE;
    static uint8_t buff_CID[SDCARD_CID_DATA_SIZE];
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(internalState == 0) {
        ret = SDCARD_ReadCardIdentification(buff_CID, sizeof(buff_CID));
        if(ret != SDCARD_ERR_NONE) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\tCID Error %ld\r\n\r\n", ret);
            return 0;
        }
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tRaw:\r\n");
        internalState++;
        printIdx = 0;
        return 1;
    } else if(internalState == 1) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\t");
        while(printIdx < sizeof(buff_CID)) {
            snprintf(tmpStr, 8, "%02x ", buff_CID[printIdx]);
            strcat(pcWriteBuffer, tmpStr);
            printIdx++;
            if((printIdx % 8) == 0) {
                strcat(pcWriteBuffer, "\r\n");
                return 1;
            }
        }
        strcat(pcWriteBuffer, "\r\n");
        internalState++;
        return 1;
    } else if(internalState == 2) {
        const uint8_t mid = buff_CID[0];
        const uint16_t oid = (((uint16_t)buff_CID[1]) << 8) + buff_CID[2];
        const char pnm[6] = {
                (char)buff_CID[3],
                (char)buff_CID[4],
                (char)buff_CID[5],
                (char)buff_CID[6],
                (char)buff_CID[7],
                '\0'
        };
        const uint8_t prv = buff_CID[8];
        const uint32_t sn = (((uint32_t)buff_CID[9]) << 24) +
                (((uint32_t)buff_CID[10]) << 16) +
                (((uint32_t)buff_CID[11]) << 8) +
                ((uint32_t)buff_CID[12]);
        const uint16_t year = 2000 + (10 * buff_CID[13] & 0x0F) + ((buff_CID[14] >> 4) & 0x0F);
        const uint8_t month = buff_CID[14] & 0x0F;

        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tManufaturer: 0x%02x\r\n"
                "\tOEM ID: 0x%04x\r\n"
                "\tName: %s\r\n"
                "\tHW Rev: %d\r\n"
                "\tFW Rev: %d\r\n"
                "\tSN: %lu\r\n"
                "\tDate: %d-%02d\r\n"
                "\r\n",
                mid,
                oid,
                pnm,
                prv >> 4, prv & 0x0F,
                sn,
                year,
                month);
        /* Done */
        internalState = 0;
    } else {
        internalState = 0;
    }
    return 0;
}


static const CLI_Command_Definition_t sdcard_cid = {
    "sd_cid",
    "sd_cid:\r\n"
    "\tCard Information Data(CID)\r\n\r\n",
    CmdSdCardCid,
    0
};


static BaseType_t CmdSdCardCsd(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    static uint32_t internalState = 0;
    static uint32_t printIdx = 0;
    char tmpStr[16];
    int32_t ret = SDCARD_ERR_NONE;
    static uint8_t buff_CSD[SDCARD_CSD_DATA_SIZE];
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(internalState == 0) {
        ret = SDCARD_ReadCardSpecificData(buff_CSD, sizeof(buff_CSD));
        if(ret != SDCARD_ERR_NONE) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\tCSD Error %ld\r\n\r\n", ret);
            return 0;
        }
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tRaw:\r\n");
        internalState++;
        printIdx = 0;
        return 1;
    } else if(internalState == 1) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\t");
        while(printIdx < sizeof(buff_CSD)) {
            snprintf(tmpStr, 8, "%02x ", buff_CSD[printIdx]);
            strcat(pcWriteBuffer, tmpStr);
            printIdx++;
            if((printIdx % 8) == 0) {
                strcat(pcWriteBuffer, "\r\n");
                return 1;
            }
        }
        strcat(pcWriteBuffer, "\r\n");
        internalState++;
        return 1;
    } else if(internalState == 2) {
        const uint32_t csdVersion = (buff_CSD[0] >> 6) & 0x03;
        const uint32_t c_size = buff_CSD[9] + ((uint32_t)buff_CSD[8] << 8) +
                ((uint32_t)(buff_CSD[7] & 0x3F) << 16);
        const uint32_t size_mb = ((c_size + 1) * 512) / 1024;
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tCSD Ver: %ld\r\n"
                "\tBlock Count: %ld\r\n"
                "\tCard Size: %ld MB\r\n"
                "\r\n",
                (csdVersion + 1),
                c_size,
                size_mb);
        internalState++;
        return 1;
    } else {
        internalState = 0;
    }
    return 0;
}


static const CLI_Command_Definition_t sdcard_csd = {
    "sd_csd",
    "sd_csd:\r\n"
    "\tCard Specific Data (CSD)\r\n\r\n",
    CmdSdCardCsd,
    0
};

static const char * strNoYes[2] = {
        "NO",
        "YES"
};

static const char * strCardPowerUpStatus[2] = {
        "BUSY",
        "READY"
};

static const char * strCardCapacityStatus[2] = {
        "SDSC",
        "SDHC/SDXC"
};

static BaseType_t CmdSdCardOcr(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    int32_t ret = SDCARD_ERR_NONE;
    uint32_t ocr = 0;
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    ret = SDCARD_ReadOCR(&ocr);
    if(ret != SDCARD_ERR_NONE) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOCR Read Error %ld\r\n\r\n", ret);
        return 0;
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tOCR: 0x%08lx\r\n"
                "\tPower Up: %s\r\n"
                "\tCapacity: %s\r\n"
                "\t1.8V: %s\r\n"
                "\tLow Voltage: %s\r\n"
                "\r\n",
                ocr,
                strCardPowerUpStatus[(ocr >> 31) & 0x1],
                strCardCapacityStatus[(ocr >> 30) & 0x1],
                strNoYes[(ocr >> 24) & 0x1],
                strNoYes[(ocr >> 7) & 0x1]);
    }
    return 0;
}

static const CLI_Command_Definition_t sdcard_ocr = {
    "sd_ocr",
    "sd_ocr:\r\n"
    "\tCard Operating Condition Register (OCR)\r\n\r\n",
    CmdSdCardOcr,
    0
};


static BaseType_t CmdSdCardReadOne(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    static uint32_t internalState = 0;
    static uint32_t printIdx = 0;
    int32_t i32Temp;
    uint32_t address;
    char tmpStr[16];
    char * ptrStrParam;
    int32_t strParamLen;
    char * ptrEnd;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(internalState == 0) {
        /* Get parameter 1 (block address) */
        ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
        memcpy(tmpStr, ptrStrParam, strParamLen);
        tmpStr[strParamLen] = '\0';
        errno = 0;
        i32Temp = strtol(tmpStr, &ptrEnd, 0);
        if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
           (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
            /* parameter is not a number */
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 1 value is invalid.\r\n\r\n");
            return 0;
        }
        address = (uint32_t)i32Temp;
        int32_t ret = SDCARD_ReadSingleBlock(address, blockData, sizeof(blockData));
        if(ret != SDCARD_ERR_NONE) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tSDCARD_ReadSingleBlock error %ld\r\n\r\n", ret);
            return 0;
        }
        internalState++;
        printIdx = 0;
        return 1;
    } else if(internalState == 1) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\t");
        while(printIdx < sizeof(blockData)) {
            snprintf(tmpStr, 8, "%02x ", blockData[printIdx]);
            strcat(pcWriteBuffer, tmpStr);
            printIdx++;
            if((printIdx % 16) == 0) {
                strcat(pcWriteBuffer, "\r\n");
                return 1;
            }
        }
        /* Done printing */
        strcat(pcWriteBuffer, "\r\n");
        internalState = 0;
        printIdx = 0;
    } else {
        internalState = 0;
    }
    return 0;
}

static const CLI_Command_Definition_t sdcard_read = {
    "sd_read",
    "sd_read <block address>:\r\n"
    "\tRead one block from SD card\r\n\r\n",
    CmdSdCardReadOne,
    1
};


static BaseType_t CmdSdCardWrite(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    static uint32_t internalState = 0;
    static int32_t nBytes = 0;
    static uint32_t blockNum = 0;
    static uint16_t offset = 0;
    static uint8_t writeData[64];
    int32_t i32Temp;
    char * ptrStrParam;
    char tmpStr[12];
    BaseType_t strParamLen;
    char * ptrEnd;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(internalState == 0) {
        /*
         * Block Number
         */
        ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
        if(ptrStrParam == NULL) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 1 not found!\r\n\r\n");
            return 0;
        }
        if(strParamLen > (sizeof(tmpStr) - 1)) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 1 len exceeded buffer!\r\n\r\n");
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
        blockNum = (uint32_t)i32Temp;
        /*
         * Offset
         */
        ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 2, &strParamLen);
        if(ptrStrParam == NULL) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 2 not found!\r\n\r\n");
            return 0;
        }
        if(strParamLen > (sizeof(tmpStr) - 1)) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 2 len exceeded buffer!\r\n\r\n");
            return 0;
        }
        memcpy(tmpStr, ptrStrParam, strParamLen);
        tmpStr[strParamLen] = '\0';
        errno = 0;
        i32Temp = strtol(tmpStr, &ptrEnd, 0);
        if((ptrEnd == tmpStr) || (*ptrEnd != '\0') ||
           (((i32Temp == LONG_MIN) || (i32Temp == LONG_MAX)) && (errno == ERANGE))) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: Parameter 2 value is invalid!\r\n\r\n");
            return 0;
        }
        offset = (uint16_t)i32Temp;
        /*
         * Data
         */
        for(nBytes = 0; nBytes < sizeof(writeData); nBytes++) {
            /* Get data */
            ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString,
                    3 + nBytes, &strParamLen);
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
            writeData[nBytes] = (uint8_t)i32Temp;
        }
        internalState++;
        return 1;
    } else if(internalState == 1) {
        int32_t ret = SDCARD_ReadSingleBlock(blockNum, blockData, sizeof(blockData));
        if(ret != SDCARD_ERR_NONE) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tSDCARD_ReadSingleBlock error %ld\r\n\r\n", ret);
            internalState = 0;
            return 0;
        }
        for(uint32_t i = 0; i < nBytes; i++) {
            blockData[offset + i] = writeData[i];
        }
        ret = SDCARD_WriteSingleBlock(blockNum, blockData, sizeof(blockData));
        if(ret != SDCARD_ERR_NONE) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tSDCARD_WriteSingleBlock error %ld\r\n\r\n", ret);
            internalState = 0;
            return 0;
        }
        internalState++;
        return 1;
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tOK\r\n\r\n");
        internalState = 0;
    }
    return 0;
}

static const CLI_Command_Definition_t sdcard_write = {
    "sd_write",
    "sd_write <block address> <offset> <data>:\r\n"
    "\tWrite one block from SD card\r\n\r\n",
    CmdSdCardWrite,
    -1
};

void TEST_SDCARD_Init(void)
{
    if(bInit) {
        return;
    }
    FreeRTOS_CLIRegisterCommand(&sdcard_init);
    FreeRTOS_CLIRegisterCommand(&sdcard_cid);
    FreeRTOS_CLIRegisterCommand(&sdcard_csd);
    FreeRTOS_CLIRegisterCommand(&sdcard_ocr);
    FreeRTOS_CLIRegisterCommand(&sdcard_read);
    FreeRTOS_CLIRegisterCommand(&sdcard_write);
    bInit = true;
}

#endif /* CONFIG_USE_SDCARD */

