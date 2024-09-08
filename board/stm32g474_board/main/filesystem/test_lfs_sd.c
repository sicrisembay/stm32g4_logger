/*
 * test_lfs_sd.c
 *
 *  Created on: Sep 5, 2024
 *      Author: Sicris Rey Embay
 */

#include "logger_conf.h"

#if CONFIG_TEST_LFS_SD

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"
#include "lfs.h"
#include "lfs_sd.h"

static bool bInit = false;

static BaseType_t FuncLfsCmdFormat(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    int32_t ret = lfs_sd_format();
    if(LFS_ERR_OK != ret) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_format %ld\r\n\r\n", ret);
        return 0;
    }
    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_format = {
    "lfs_format",
    "lfs_format:\r\n"
    "\tFormats the storage device\r\n\r\n",
    FuncLfsCmdFormat,
    0
};


static BaseType_t FuncLfsCmdMount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(lfs_sd_mount()) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_mount\r\n\r\n");
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_mount = {
    "lfs_mount",
    "lfs_mount:\r\n"
    "\tMounts the storage device\r\n\r\n",
    FuncLfsCmdMount,
    0
};


static BaseType_t FuncLfsCmdUmount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(LFS_ERR_OK == lfs_sd_umount()) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_umount\r\n\r\n");
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_umount = {
    "lfs_umount",
    "lfs_umount:\r\n"
    "\tUnmounts the storage device\r\n\r\n",
    FuncLfsCmdUmount,
    0
};


static BaseType_t FuncLfsCmdDf(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    const int32_t nAllocBlock = lfs_sd_df();
    const int32_t nTotalBlock = lfs_sd_capacity();
    if((nAllocBlock < 0) || (nTotalBlock <= 0)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_df\r\n\r\n");
        return 0;
    }
    const int32_t nFreeBlock = nTotalBlock - nAllocBlock;
    const int32_t used = (nAllocBlock * 100) / nTotalBlock;
    snprintf(pcWriteBuffer, xWriteBufferLen,
            "\tFree Blocks: %ld\r\n"
            "\tUsed: %ld/%ld (%ld%%)\r\n\r\n",
            nFreeBlock,
            nAllocBlock,
            nTotalBlock,
            used);
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_df = {
    "lfs_df",
    "lfs_df:\r\n"
    "\tDisk free space\r\n\r\n",
    FuncLfsCmdDf,
    0
};

static BaseType_t FuncLfsCmdLs(
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
        if(LFS_ERR_OK != lfs_sd_ls("/", pcWriteBuffer, xWriteBufferLen)) {
            strncat(pcWriteBuffer, "\tFailed\r\n\r\n", xWriteBufferLen);
        }
    } else {
        if(LFS_ERR_OK != lfs_sd_ls(ptrStrParam, pcWriteBuffer, xWriteBufferLen)) {
            strncat(pcWriteBuffer, "\tFailed\r\n\r\n", xWriteBufferLen);
        }
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_ls = {
    "lfs_ls",
    "lfs_ls <directory>:\r\n"
    "\tList the contents of directory\r\n\r\n",
    FuncLfsCmdLs,
    -1
};


static BaseType_t FuncLfsCmdMkDir(
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
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: Parameter1 not found\r\n\r\n");
        return 0;
    }

    if(LFS_ERR_OK != lfs_sd_mkdir(ptrStrParam)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: mkdir\r\n\r\n");
        return 0;
    }

    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_mkdir = {
    "lfs_mkdir",
    "lfs_mkdir <directory>:\r\n"
    "\tCreates a directory\r\n\r\n",
    FuncLfsCmdMkDir,
    1
};


static BaseType_t FuncLfsCmdFopen(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get file name */
    ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(NULL == ptrStrParam) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: Parameter1 not found!\r\n\r\n");
        return 0;
    }

    if(NULL != lfs_sd_fopen(ptrStrParam)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: open failed!\r\n\r\n");
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_fopen = {
    "lfs_fopen",
    "lfs_fopen <filename>:\r\n"
    "\tOpen file in RDWR mode\r\n\r\n",
    FuncLfsCmdFopen,
    1
};


static BaseType_t FuncLfsCmdFclose(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(LFS_ERR_OK != lfs_sd_fclose()) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: fclose\r\n\r\n");
        return 0;
    }
    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_fclose = {
    "lfs_fclose",
    "lfs_fclose:\r\n"
    "\tCloses already opened file\r\n\r\n",
    FuncLfsCmdFclose,
    0
};


void TEST_LFS_Init(void)
{
    if(bInit) {
        return;
    }

    FreeRTOS_CLIRegisterCommand(&lfs_cmd_format);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_mount);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_umount);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_df);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_ls);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_mkdir);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_fopen);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_fclose);

    bInit = true;
}

#endif /* CONFIG_TEST_LFS_SD */
