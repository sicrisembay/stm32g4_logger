/*
 * lfs_sd.c
 *
 *  Created on: Sep 4, 2024
 *      Author: Sicris Rey Embay
 */

#include "logger_conf.h"

#if CONFIG_USE_LFS_SD

#include "FreeRTOS.h"
#include "semphr.h"
#include "lfs.h"
#include "lfs_sd.h"
#include "sdcard.h"

#define LFS_LOOKAHEAD_SIZE          (16)
#define LFS_SD_PRINT_DEBUG_ENABLE   (1)
#define LFS_SD_PRINTF(x, ...)       (LFS_SD_PRINT_DEBUG_ENABLE != 0) ? LPUART_printf("lfs_sd: "x, ##__VA_ARGS__) : (void)0

static struct lfs_config cfg = {0};
static struct lfs_file_config cfgFile = {0};
static lfs_file_t file = {0};
static lfs_t lfs;
static bool bInit = false;
static bool bMount = false;
static bool bFileOpen = false;
static uint8_t lfs_readBuffer[SDCARD_BLOCK_SIZE];
static uint8_t lfs_progBuffer[SDCARD_BLOCK_SIZE];
static uint8_t lfs_lookAheadBuffer[LFS_LOOKAHEAD_SIZE];
static uint8_t dummyBuffer[SDCARD_BLOCK_SIZE];
static uint8_t fileCacheBuffer[SDCARD_BLOCK_SIZE];

#ifdef LFS_THREADSAFE
static SemaphoreHandle_t lfs_mutex_handle = NULL;
static StaticSemaphore_t lfs_mutexStruct;
#endif /* LFS_THREADSAFE */

static int sd_read(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size)
{
    if((c == NULL) || (buffer == NULL)) {
        return LFS_ERR_INVAL;
    }
    (void)size;  // unused
    (void)off;   // unused

    const int32_t ret = SDCARD_ReadSingleBlock(block, buffer, c->read_size);
    if(SDCARD_ERR_NONE != ret) {
        LFS_SD_PRINTF("SD read error %ld\r\n", ret);
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}


static int sd_prog(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size)
{
    if((c == NULL) || (buffer == NULL)) {
        return LFS_ERR_INVAL;
    }
    (void)size;  // unused
    (void)off;   // unused

    const int32_t ret = SDCARD_WriteSingleBlock(block, buffer, c->prog_size);
    if(SDCARD_ERR_NONE != ret) {
        LFS_SD_PRINTF("SD write error %ld\r\n", ret);
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}


static int sd_erase(const struct lfs_config *c, lfs_block_t block)
{
    memset(dummyBuffer, 0xFF, sizeof(dummyBuffer));
    // SD dummy erase
    const int32_t ret = SDCARD_WriteSingleBlock(block, dummyBuffer, sizeof(dummyBuffer));
    if(SDCARD_ERR_NONE != ret) {
        LFS_SD_PRINTF("SD erase error %ld\r\n", ret);
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}


static int sd_sync(const struct lfs_config *c)
{
    return LFS_ERR_OK;
}



#ifdef LFS_THREADSAFE
#define LFS_MUTEX_DEFAULT_TIMEOUT       (200)

static int sd_lock(const struct lfs_config *c)
{
    BaseType_t taskWoken = pdFALSE;
    BaseType_t success = pdFALSE;
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    if(bInsideISR) {
        success = xSemaphoreTakeFromISR(lfs_mutex_handle, &taskWoken);
        if(pdTRUE == success) {
            portYIELD_FROM_ISR(taskWoken);
        }
    } else {
        success = xSemaphoreTake(lfs_mutex_handle, LFS_MUTEX_DEFAULT_TIMEOUT);
    }

    if(pdTRUE == success) {
        return LFS_ERR_OK;
    } else {
        return LFS_ERR_IO;
    }
}

static int sd_unlock(const struct lfs_config *c)
{
    BaseType_t taskWoken = pdFALSE;
    BaseType_t success = pdFALSE;
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    if(bInsideISR) {
        success = xSemaphoreGiveFromISR(lfs_mutex_handle, &taskWoken);
        if(pdTRUE == success) {
            portYIELD_FROM_ISR(taskWoken);
        }
    } else {
        success = xSemaphoreGive(lfs_mutex_handle);
    }

    if(pdTRUE == success) {
        return LFS_ERR_OK;
    } else {
        return LFS_ERR_IO;
    }
}

#endif /* LFS_THREADSAFE */

static void lfs_sd_init(void)
{
    if(bInit) {
        return;
    }

    /*
     * Note: This must be only called after scheduler has started.
     */
    configASSERT(taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState());

    /*
     * Wait for SD card to be initialized
     */
    while(SDCARD_InitDone() != true) {
        vTaskDelay(10);
    }

#ifdef LFS_THREADSAFE
    lfs_mutex_handle = xSemaphoreCreateMutexStatic(&lfs_mutexStruct);
    configASSERT(lfs_mutex_handle != NULL);
#endif /* LFS_THREADSAFE */

    cfg.context = NULL;
    cfg.read = sd_read;
    cfg.prog = sd_prog;
    cfg.erase = sd_erase;
    cfg.sync = sd_sync;
#ifdef LFS_THREADSAFE
    cfg.lock = sd_lock;
    cfg.unlock = sd_unlock;
#endif /* LFS_THREADSAFE */
    cfg.read_size = SDCARD_BLOCK_SIZE;
    cfg.prog_size = SDCARD_BLOCK_SIZE;
    cfg.block_size = SDCARD_BLOCK_SIZE;
    cfg.block_count = SDCARD_GetBlockCount();
    cfg.block_cycles = 500;
    cfg.cache_size = SDCARD_BLOCK_SIZE;
    cfg.lookahead_size = LFS_LOOKAHEAD_SIZE;
    cfg.read_buffer = lfs_readBuffer;
    cfg.prog_buffer = lfs_progBuffer;
    cfg.lookahead_buffer = lfs_lookAheadBuffer;

    memset(&cfgFile, 0, sizeof(cfgFile));
    cfgFile.buffer = fileCacheBuffer;
    cfgFile.attr_count = 0;

    bInit = true;
}


int32_t lfs_sd_format()
{
    if(bInit != true) {
        lfs_sd_init();
    }
    if(bMount) {
        lfs_sd_umount();
    }

    return lfs_format(&lfs, &cfg);
}


lfs_t * lfs_sd_mount()
{
    if(bInit != true) {
        lfs_sd_init();
    }

    const int32_t retMount = lfs_mount(&lfs, &cfg);
    if(LFS_ERR_OK != retMount) {
        LFS_SD_PRINTF("Mount failed %ld\r\n", retMount);
        return NULL;
    }
    bMount = true;
    return &lfs;
}


int32_t lfs_sd_umount()
{
    int32_t ret = LFS_ERR_OK;

    if(bInit != true) {
        LFS_SD_PRINTF("LFS not initialized\r\n");
        return LFS_ERR_IO;
    }

    if(bMount) {
        if(bFileOpen) {
            lfs_sd_fclose();
        }
        ret = lfs_unmount(&lfs);
        bMount = false;
    }
    return ret;
}


int32_t lfs_sd_df()
{
    if(bMount != true) {
        LFS_SD_PRINTF("Storage device not mounted.\r\n");
        return LFS_ERR_IO;
    }
    return lfs_fs_size(&lfs);
}


int32_t lfs_sd_capacity()
{
    if(bMount != true) {
        LFS_SD_PRINTF("Storage device not mounted.\r\n");
        return LFS_ERR_IO;
    }
    return (cfg.block_count);
}


int32_t lfs_sd_mkdir(const char * path)
{
    if(path == NULL) {
        return LFS_ERR_INVAL;
    }

    if(bMount != true) {
        LFS_SD_PRINTF("Storage device not mounted.\r\n");
        return LFS_ERR_IO;
    }
    return lfs_mkdir(&lfs, path);
}


int32_t lfs_sd_ls(const char * path, char * outBuffer, size_t bufferLen)
{
    char tempStr[24];
    if((path == NULL) || (outBuffer == NULL)) {
        return LFS_ERR_INVAL;
    }
    if(bMount != true) {
        LFS_SD_PRINTF("Storage device not mounted.\r\n");
        return LFS_ERR_IO;
    }

    lfs_dir_t dir;
    struct lfs_info info;
    int32_t err = lfs_dir_open(&lfs, &dir, path);
    if(err) {
        LFS_SD_PRINTF("lfs_dir_open error %ld\r\n", err);
        return err;
    }

    while(1) {
        int32_t res = lfs_dir_read(&lfs, &dir, &info);
        if(res < 0) {
            LFS_SD_PRINTF("lfs_dir_read error %ld\r\n", res);
            return res;
        }

        if(res == 0) {
            break;
        }

        switch (info.type) {
            case LFS_TYPE_REG: {
                outBuffer = strncat(outBuffer, "--- ", bufferLen);
                break;
            }
            case LFS_TYPE_DIR: {
                outBuffer = strncat(outBuffer, "--d ", bufferLen);
                break;
            }
            default: {
                outBuffer = strncat(outBuffer, "--? ", bufferLen);
                break;
            }
        }

        snprintf(tempStr, sizeof(tempStr), "%10ld ", info.size);
        strncat(outBuffer, tempStr, bufferLen);
        snprintf(tempStr, sizeof(tempStr), "%s\r\n", info.name);
        strncat(outBuffer, tempStr, bufferLen);
    }

    strncat(outBuffer, "\r\n", bufferLen);
    err = lfs_dir_close(&lfs, &dir);
    if(err) {
        LFS_SD_PRINTF("lfs_dir_close error %ld\r\n", err);
        return err;
    }

    return LFS_ERR_OK;
}


lfs_file_t * lfs_sd_fopen(const char * pathName)
{
    lfs_file_t * ret = NULL;

    if(NULL == pathName) {
        LFS_SD_PRINTF("Error: invalid arg\r\n");
        return NULL;
    }
    if(bMount != true) {
        LFS_SD_PRINTF("Error: LFS not mounted\r\n");
        return NULL;
    }
    if(bFileOpen) {
        LFS_SD_PRINTF("Error: Previous file still open.\r\n");
        return NULL;
    }

    if(LFS_ERR_OK == lfs_file_opencfg(&lfs, &file, pathName, LFS_O_CREAT | LFS_O_RDWR, &cfgFile)) {
        bFileOpen = true;
        ret = &file;
    } else {
        bFileOpen = false;
    }
    return ret;
}


int32_t lfs_sd_fwrite(const char * strData, size_t len)
{
    if((NULL == strData) || (len == 0)) {
        LFS_SD_PRINTF("Error: invalid arg\r\n");
        return LFS_ERR_INVAL;
    }
    if(bMount != true) {
        LFS_SD_PRINTF("Error: LFS not mounted\r\n");
        return LFS_ERR_IO;
    }
    if(bFileOpen != true) {
        LFS_SD_PRINTF("Error: File not opened\r\n");
        return LFS_ERR_IO;
    }
    return lfs_file_write(&lfs, &file, strData, len);
}


int32_t lfs_sd_fread(char * outBuffer, size_t bufLen)
{
    if((NULL == outBuffer) || (bufLen == 0)) {
        LFS_SD_PRINTF("Error: invalid arg\r\n");
        return LFS_ERR_INVAL;
    }
    if(bMount != true) {
        LFS_SD_PRINTF("Error: LFS not mounted\r\n");
        return LFS_ERR_IO;
    }
    if(bFileOpen != true) {
        LFS_SD_PRINTF("Error: File not opened\r\n");
        return LFS_ERR_IO;
    }
    return lfs_file_read(&lfs, &file, outBuffer, bufLen);
}


int32_t lfs_sd_mv(const char * source, const char * target)
{
    if((NULL == source) || (NULL == target)) {
        LFS_SD_PRINTF("Error: invalid arg\r\n");
        return LFS_ERR_INVAL;
    }
    if(bMount != true) {
        LFS_SD_PRINTF("Error: LFS not mounted\r\n");
        return LFS_ERR_IO;
    }

    return lfs_rename(&lfs, source, target);
}


int32_t lfs_sd_rm(const char * path)
{
    if(NULL == path) {
        LFS_SD_PRINTF("Error: invalid arg\r\n");
        return LFS_ERR_INVAL;
    }
    if(bMount != true) {
        LFS_SD_PRINTF("Error: LFS not mounted\r\n");
        return LFS_ERR_IO;
    }
    return lfs_remove(&lfs, path);
}


int32_t lfs_sd_fclose()
{
    int32_t ret = LFS_ERR_OK;
    if((bMount == false) || (bFileOpen == false)) {
        return LFS_ERR_IO;
    }

    ret = lfs_file_close(&lfs, &file);
    memset(&file, 0, sizeof(file));
    bFileOpen = false;
    return ret;
}


uint32_t lfs_crc(uint32_t crc, const void *buffer, size_t size) {
    static const uint32_t rtable[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
        0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
    };

    const uint8_t *data = buffer;

    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 0)) & 0xf];
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 4)) & 0xf];
    }

    return crc;
}

#endif /* CONFIG_USE_LFS_SD */

