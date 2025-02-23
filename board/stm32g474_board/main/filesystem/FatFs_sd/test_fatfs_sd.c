/*
 * test_fatfs_sd.c
 *
 *  Created on: Feb 22, 2025
 *      Author: H255182
 */

#include "logger_conf.h"

#if CONFIG_TEST_FATFS_SD

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"
#include "../../components/filesystem/FatFs/source/ff.h"
#include "fatfs_sd.h"


static bool bInit = false;

#if CONFIG_FATFS_CMD_FORMAT
static BaseType_t FuncFatFsFormat(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    FRESULT result = FR_OK;
    memset(pcWriteBuffer, 0, xWriteBufferLen);

//    result = fatfs_sd_format();
    if(result == FR_OK) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: fatfs_sd_mount %d\r\n\r\n", result);
    }
    return 0;
}


static const CLI_Command_Definition_t fatfs_cmd_format = {
    "fatfs_format",
    "fatfs_format:\r\n"
    "\tFormats the storage device\r\n\r\n",
    FuncFatFsFormat,
    0
};
#endif /* CONFIG_FATFS_CMD_FORMAT */

#if CONFIG_FATFS_CMD_MOUNT
static BaseType_t FuncFatFsCmdMount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    FRESULT result;
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    result = fatfs_sd_mount();
    if(result == FR_OK) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: fatfs_sd_mount %d\r\n\r\n", result);
    }
    return 0;
}


static const CLI_Command_Definition_t fatfs_cmd_mount = {
    "fatfs_mount",
    "fatfs_mount:\r\n"
    "\tMounts the storage device\r\n\r\n",
    FuncFatFsCmdMount,
    0
};
#endif /* CONFIG_FATFS_CMD_MOUNT */

#if CONFIG_FATFS_CMD_INFO
char * strFatFsType(BYTE type)
{
    char * ret;
    switch(type) {
        case FS_FAT12: {
            ret = "FAT12";
            break;
        }
        case FS_FAT16: {
            ret = "FAT16";
            break;
        }
        case FS_FAT32: {
            ret = "FAT32";
            break;
        }
        case FS_EXFAT: {
            ret = "EXFAT";
            break;
        }
        default: {
            ret = "Invalid FAT";
            break;
        }
    }
    return ret;
}

static BaseType_t FuncFatFsCmdInfo(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    FATFS const * const instance = fatfs_get_instance();
    if(NULL == instance) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: Invalid FAT file system\r\n\r\n");
        return 0;
    }

    snprintf(pcWriteBuffer, xWriteBufferLen,
            "\tType: %s\r\n"
            "\tFAT entry: %ld\r\n"
            "\tSector/FAT: %ld\r\n"
            "\tCluster size: %d\r\n"
            "\tBase Sectors\r\n"
            "\t  Volume: 0x%08lx\r\n"
            "\t  FAT   : 0x%08lx\r\n"
            "\t  RDIR  : 0x%08lx\r\n"
            "\t  DATA  : 0x%08lx\r\n"
            "\tVolume\r\n"
            "\t  label: %s\r\n"
            "\t  SN: %ld\r\n"
            "\r\n",
            strFatFsType(instance->fs_type),
            instance->n_fatent,
            instance->fsize,
            instance->csize,
            instance->volbase,
            instance->fatbase,
            instance->dirbase,
            instance->database,
            fatfs_get_volume_label(),
            fatfs_get_volume_serial_number()
            );

    return 0;
}

static const CLI_Command_Definition_t fatfs_cmd_info = {
    "fatfs_info",
    "fatfs_info:\r\n"
    "\tPrints FAT filesytem information\r\n\r\n",
    FuncFatFsCmdInfo,
    0
};
#endif

#if CONFIG_FATFS_CMD_LS
static BaseType_t FuncFatFsCmdLs(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get directory name */
    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(NULL == ptrStrParam) {
        if(FR_OK != fatfs_sd_ls("/", pcWriteBuffer, xWriteBufferLen)) {
            strncat(pcWriteBuffer, "\tFailed\r\n\r\n", xWriteBufferLen);
            return 0;
        }
    } else {
        if(FR_OK != fatfs_sd_ls(ptrStrParam, pcWriteBuffer, xWriteBufferLen)) {
            strncat(pcWriteBuffer, "\tFailed\r\n\r\n", xWriteBufferLen);
            return 0;
        }
    }
    strncat(pcWriteBuffer, "\r\n", xWriteBufferLen - strlen(pcWriteBuffer));
    return 0;
}


static const CLI_Command_Definition_t fatfs_cmd_ls = {
    "fatfs_ls",
    "fatfs_ls <directory>:\r\n"
    "\tList the contents of directory\r\n\r\n",
    FuncFatFsCmdLs,
    -1
};
#endif /* CONFIG_FATFS_CMD_LS */

void TEST_FatFS_Init(void)
{
    if(bInit) {
        return;
    }

#if CONFIG_FATFS_CMD_FORMAT
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_format);
#endif

#if CONFIG_FATFS_CMD_MOUNT
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_mount);
#endif

#if CONFIG_FATFS_CMD_INFO
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_info);
#endif

#if CONFIG_FATFS_CMD_LS
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_ls);
#endif
    bInit = true;
}

#else
void TEST_FatFS_Init(void)
{

}
#endif /* CONFIG_TEST_FATFS_SD */
