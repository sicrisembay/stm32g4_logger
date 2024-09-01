/*
 * sdcard.c
 *
 *  Created on: Aug 31, 2024
 *      Author: Sicris Rey Embay
 */

#include "stdbool.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_bus.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sdcard.h"
#include "bsp/spi/bsp_spi.h"

#define SD_PRINT_DEBUG_ENABLE   (1)
#define SD_PRINTF(x, ...)       (SD_PRINT_DEBUG_ENABLE != 0) ? LPUART_printf(x, ##__VA_ARGS__) : (void)0

#define SD_SPI_PORT             SPI1
#define SD_CS_Pin               LL_GPIO_PIN_4
#define SD_CS_Port              GPIOA

#define SD_DETECT_Pin           LL_GPIO_PIN_4
#define SD_DETECT_Port          GPIOC

#define SD_DEFAULT_TIMEOUT      (100)

static bool bInit = false;
static SemaphoreHandle_t semHandle = NULL;
static StaticSemaphore_t semStruct;

void SD_ChipSelect(bool bSelect)
{
    if(bSelect) {
        /* Active Low */
        LL_GPIO_ResetOutputPin(SD_CS_Port, SD_CS_Pin);
    } else {
        LL_GPIO_SetOutputPin(SD_CS_Port, SD_CS_Pin);
    }
}


/*
R1: 0abcdefg
     ||||||`- 1th bit (g): card is in idle state
     |||||`-- 2th bit (f): erase sequence cleared
     ||||`--- 3th bit (e): illigal command detected
     |||`---- 4th bit (d): crc check error
     ||`----- 5th bit (c): error in the sequence of erase commands
     |`------ 6th bit (b): misaligned addres used in command
     `------- 7th bit (a): command argument outside allowed range
             (8th bit is always zero)
*/
static int8_t SDCARD_ReadR1() {
    int8_t ret = SPI_ERR_NONE;
    int32_t spiRet = SPI_ERR_NONE;
    int32_t status;
    uint8_t r1;
    // make sure FF is transmitted during receive
    uint8_t tx = 0xFF;
    for(;;) {
        // clear pending semaphore
        while(pdTRUE == xSemaphoreTake(semHandle, 0));

        spiRet = BSP_SPI_transact(&tx, &r1, 1, SPI_MODE0, NULL, semHandle, &status);
        if(spiRet != SPI_ERR_NONE) {
            return (int8_t)spiRet;
        }

        if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
            return SPI_ERR_TIMEOUT;
        }
        if(status != SPI_ERR_NONE) {
            return (int8_t)spiRet;
        }

        if((r1 & 0x80) == 0) { // 8th bit alwyas zero, r1 recevied
            ret = r1;
            break;
        }
    }

    return ret;
}

// data token for CMD9, CMD17, CMD18 and CMD24 are the same
#define DATA_TOKEN_CMD9  0xFE
#define DATA_TOKEN_CMD17 0xFE
#define DATA_TOKEN_CMD18 0xFE
#define DATA_TOKEN_CMD24 0xFE
#define DATA_TOKEN_CMD25 0xFC

#if 0
static int SDCARD_WaitDataToken(uint8_t token) {
    uint8_t fb;
    // make sure FF is transmitted during receive
    uint8_t tx = 0xFF;
    for(;;) {
        HAL_SPI_TransmitReceive(&SDCARD_SPI_PORT, &tx, &fb, sizeof(fb), HAL_MAX_DELAY);
        if(fb == token)
            break;

        if(fb != 0xFF)
            return -1;
    }
    return 0;
}
#endif

static int32_t SDCARD_ReadBytes(uint8_t * buff, size_t buff_size) {
#if 0
    // make sure FF is transmitted during receive
    uint8_t tx = 0xFF;
    while(buff_size > 0) {
        int32_t status = 0;
        BSP_SPI_transact(&tx, buff, 1, SPI_MODE0, NULL, semHandle, &status);
        if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
            return SDCARD_ERR_TIMEOUT;
        }
        if(status < 0) {
            return status;
        }
        buff++;
        buff_size--;
    }
#else
    int32_t ret = 0;
    int32_t status = 0;
    memset(buff, 0xFF, buff_size);
    /* clear */
    while(pdTRUE == xSemaphoreTake(semHandle, 0));
    ret = BSP_SPI_transact(buff, buff, buff_size, SPI_MODE0, NULL, semHandle, &status);
    if(SPI_ERR_NONE != ret) {
        return ret;
    }
    if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
        return SPI_ERR_TIMEOUT;
    }
    if(status < 0) {
        return status;
    }
#endif
    return SPI_ERR_NONE;
}


static int32_t SDCARD_WaitNotBusy() {
    uint8_t busy;
    int32_t ret;
    do {
        ret = SDCARD_ReadBytes(&busy, sizeof(busy));
        if(ret != SPI_ERR_NONE) {
            return ret;
        }
    } while(busy != 0xFF);

    return SPI_ERR_NONE;
}


int32_t SDCARD_Init(void)
{
    int32_t ret = SPI_ERR_NONE;
    int32_t status;
    int8_t r1;

    /*
     * Note: This must be only called after scheduler has started.
     */
    configASSERT(taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState());

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    if(bInit == true) {
        SD_PRINTF("SD Init Error %d\r\n", __LINE__);
        return SDCARD_ERR_NONE;
    }

    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);

    BSP_SPI_init();

    /*
     * SD Card SPI CS
     */
    LL_GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.Pin = SD_CS_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(SD_CS_Port, &GPIO_InitStruct);
    SD_ChipSelect(false);
    /*
     * SD Card Detect
     */
    LL_GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.Pin = SD_DETECT_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(SD_DETECT_Port, &GPIO_InitStruct);
    if((LL_GPIO_ReadInputPort(SD_DETECT_Port) & SD_DETECT_Pin) == 0) {
        /* Card not detected.  Hot plug is not supported */
        SD_PRINTF("SD Init Error %d\r\n", __LINE__);
        return SDCARD_ERR_NOT_PRESENT;
    }
    /*
     * Semaphore for synchronization
     */
    semHandle = xSemaphoreCreateBinaryStatic(&semStruct);
    configASSERT(NULL != semHandle);
    /*
     * Step 0.
     *   Wait for at least 1ms
     */
    vTaskDelay(2/portTICK_PERIOD_MS);
    /*
     * Step 1.
     *   Set DI and CS high and apply 74 or more clock pulses to SCLK. Without this
     *   step under certain circumstances SD-card will not work. For instance, when
     *   multiple SPI devices are sharing the same bus (i.e. MISO, MOSI, CS).
    */
    SD_ChipSelect(false);  // CS unselect
    {
        // 80 clock pulses
        uint8_t dummy[10] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                0xFF, 0xFF};
        while(pdTRUE == xSemaphoreTake(semHandle, 0));
        ret = BSP_SPI_transact(dummy, dummy, 10, SPI_MODE0, NULL, semHandle, &status);
        if(ret != SPI_ERR_NONE) {
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return SPI_ERR_TIMEOUT;
        }
    }

    /*
     * Step 2.
     *   Send CMD0 (GO_IDLE_STATE): Reset the SD card.
    */
    SD_ChipSelect(true);
    ret = SDCARD_WaitNotBusy();
    if(ret != SPI_ERR_NONE) {
        SD_ChipSelect(false);
        SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
        return ret;
    }
    {
        uint8_t cmd[] =
            { 0x40 | 0x00 /* CMD0 */, 0x00, 0x00, 0x00, 0x00 /* ARG = 0 */, (0x4A << 1) | 1 /* CRC7 + end bit */ };
        while(pdTRUE == xSemaphoreTake(semHandle, 0));
        ret = BSP_SPI_transact(cmd, cmd, sizeof(cmd), SPI_MODE0, NULL, semHandle, &status);
        if(ret != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
            return ret;
        }
        if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
            return SPI_ERR_TIMEOUT;
        }
        if(status != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
            return status;
        }
    }

    r1 = SDCARD_ReadR1();
    if(r1 < 0) {
        SD_ChipSelect(false);
        ret = r1;
        SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
        return ret;
    }
    if(r1 != 0x01) {
        SD_ChipSelect(false);
        SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
        return SDCARD_ERR_UNKNOWN_CARD;
    }

    /*
     * Step 3.
     *   After the card enters idle state with a CMD0, send a CMD8 with argument of
     *   0x000001AA and correct CRC prior to initialization process. If the CMD8 is
     *   rejected with illegal command error (0x05), the card is SDC version 1 or
     *   MMC version 3. If accepted, R7 response (R1(0x01) + 32-bit return value)
     *   will be returned. The lower 12 bits in the return value 0x1AA means that
     *   the card is SDC version 2 and it can work at voltage range of 2.7 to 3.6 volts.
     *   If not the case, the card should be rejected.
    */
    ret = SDCARD_WaitNotBusy();
    if(ret != SPI_ERR_NONE) {
        SD_ChipSelect(false);
        SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
        return ret;
    }
    {
        uint8_t cmd[] =
            { 0x40 | 0x08 /* CMD8 */, 0x00, 0x00, 0x01, 0xAA /* ARG */, (0x43 << 1) | 1 /* CRC7 + end bit */ };
        while(pdTRUE == xSemaphoreTake(semHandle, 0));
        ret = BSP_SPI_transact(cmd, cmd, sizeof(cmd), SPI_MODE0, NULL, semHandle, &status);
        if(ret != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
            return ret;
        }
        if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
            return SPI_ERR_TIMEOUT;
        }
        if(status != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
            return status;
        }
    }

    r1 = SDCARD_ReadR1();
    if(r1 < 0) {
        SD_ChipSelect(false);
        ret = r1;
        SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
        return ret;
    }
    if(r1 == 0x05) {
        // SD Ver1 or MMC Ver3
        SD_PRINTF("SD Init  Error %d\r\n", __LINE__);
        return SDCARD_ERR_UNSUPPORTED;  // SDSC not supported
    } else if(r1 == 0x01) {
        uint8_t resp[4];
        ret = SDCARD_ReadBytes(resp, sizeof(resp));
        if(ret != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        if(((resp[2] & 0x01) != 1) || (resp[3] != 0xAA)) {
            /* 0x1AA mismatch */
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return SDCARD_ERR_UNKNOWN_CARD;
        }
    } else {
        SD_ChipSelect(false);
        SD_PRINTF("SD Init Error %d\r\n", __LINE__);
        return SDCARD_ERR_UNKNOWN_CARD;
    }
    /*
     * Step 4.
     *   And then initiate initialization with ACMD41 with HCS flag (bit 30).
     */
    while(1) {
        ret = SDCARD_WaitNotBusy();
        if(ret != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        {
            uint8_t cmd[] =
                { 0x40 | 0x37 /* CMD55 */, 0x00, 0x00, 0x00, 0x00 /* ARG */, (0x7F << 1) | 1 /* CRC7 + end bit */ };
            while(pdTRUE == xSemaphoreTake(semHandle, 0));
            ret = BSP_SPI_transact(cmd, cmd, sizeof(cmd), SPI_MODE0, NULL, semHandle, &status);
            if(ret != SPI_ERR_NONE) {
                SD_ChipSelect(false);
                SD_PRINTF("SD Init Error %d\r\n", __LINE__);
                return ret;
            }
            if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
                SD_ChipSelect(false);
                SD_PRINTF("SD Init Error %d\r\n", __LINE__);
                return SPI_ERR_TIMEOUT;
            }
            if(status != SPI_ERR_NONE) {
                SD_ChipSelect(false);
                SD_PRINTF("SD Init Error %d\r\n", __LINE__);
                return status;
            }
        }

        r1 = SDCARD_ReadR1();
        if(r1 < 0) {
            SD_ChipSelect(false);
            ret = r1;
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        if(r1 != 0x01) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return SDCARD_ERR_R1;
        }
        ret = SDCARD_WaitNotBusy();
        if(ret != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        {
            uint8_t cmd[] =
                { 0x40 | 0x29 /* ACMD41 */, 0x40, 0x00, 0x00, 0x00 /* ARG */, (0x7F << 1) | 1 /* CRC7 + end bit */ };
            while(pdTRUE == xSemaphoreTake(semHandle, 0));
            ret = BSP_SPI_transact(cmd, cmd, sizeof(cmd), SPI_MODE0, NULL, semHandle, &status);
            if(ret != SPI_ERR_NONE) {
                SD_ChipSelect(false);
                SD_PRINTF("SD Init Error %d\r\n", __LINE__);
                return ret;
            }
            if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
                SD_ChipSelect(false);
                SD_PRINTF("SD Init Error %d\r\n", __LINE__);
                return SPI_ERR_TIMEOUT;
            }
            if(status != SPI_ERR_NONE) {
                SD_ChipSelect(false);
                SD_PRINTF("SD Init Error %d\r\n", __LINE__);
                return status;
            }
        }

        r1 = SDCARD_ReadR1();
        if(r1 < 0) {
            SD_ChipSelect(false);
            ret = r1;
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        if(r1 == 0x00) {
            break;
        }
        if(r1 != 0x01) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return SDCARD_ERR_R1;
        }
    }
    /*
     * Step 5.
     *   After the initialization completed, read OCR register with CMD58 and check
     *   CCS flag (bit 30). When it is set, the card is a high-capacity card known
     *   as SDHC/SDXC.
     */
    ret = SDCARD_WaitNotBusy();
    if(ret != SPI_ERR_NONE) {
        SD_ChipSelect(false);
        SD_PRINTF("SD Init Error %d\r\n", __LINE__);
        return ret;
    }
    {
        uint8_t cmd[] =
            { 0x40 | 0x3A /* CMD58 */, 0x00, 0x00, 0x00, 0x00 /* ARG */, (0x7F << 1) | 1 /* CRC7 + end bit */ };
        while(pdTRUE == xSemaphoreTake(semHandle, 0));
        ret = BSP_SPI_transact(cmd, cmd, sizeof(cmd), SPI_MODE0, NULL, semHandle, &status);
        if(ret != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        if(pdTRUE != xSemaphoreTake(semHandle, SD_DEFAULT_TIMEOUT)) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return SPI_ERR_TIMEOUT;
        }
        if(status != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return status;
        }
    }

    r1 = SDCARD_ReadR1();
    if(r1 < 0) {
        SD_ChipSelect(false);
        ret = r1;
        SD_PRINTF("SD Init Error %d\r\n", __LINE__);
        return ret;
    }
    if(r1 != 0x00) {
        SD_ChipSelect(false);
        SD_PRINTF("SD Init Error %d\r\n", __LINE__);
        return SDCARD_ERR_R1;
    }
    {
        uint8_t resp[4];
        ret = SDCARD_ReadBytes(resp, sizeof(resp));
        if(ret != SPI_ERR_NONE) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return ret;
        }
        /*
         * OCR
         * bit31: Card Power Up Status (0: busy, 1: Ready)
         * bit30: Card Capacity Status (0: SDSC, 1: SDHC/SDXC)
         * bit29: UHS-II Card Status (0: NO, 1: YES)
         */
        if((resp[0] & 0xC0) != 0xC0) {
            SD_ChipSelect(false);
            SD_PRINTF("SD Init Error %d\r\n", __LINE__);
            return SDCARD_ERR_UNSUPPORTED;
        }
    }

    SD_ChipSelect(false);

    bInit = true;

    return SDCARD_ERR_NONE;
}

#if 0
int SDCARD_GetBlocksNumber(uint32_t* num) {
    uint8_t csd[16];
    uint8_t crc[2];

    SDCARD_Select();

    if(SDCARD_WaitNotBusy() < 0) { // keep this!
        SDCARD_Unselect();
        return -1;
    }

    /* CMD9 (SEND_CSD) command */
    {
        static const uint8_t cmd[] =
            { 0x40 | 0x09 /* CMD9 */, 0x00, 0x00, 0x00, 0x00 /* ARG */, (0x7F << 1) | 1 /* CRC7 + end bit */ };
        HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)cmd, sizeof(cmd), HAL_MAX_DELAY);
    }

    if(SDCARD_ReadR1() != 0x00) {
        SDCARD_Unselect();
        return -2;
    }

    if(SDCARD_WaitDataToken(DATA_TOKEN_CMD9) < 0) {
        SDCARD_Unselect();
        return -3;
    }

    if(SDCARD_ReadBytes(csd, sizeof(csd)) < 0) {
        SDCARD_Unselect();
        return -4;
    }

    if(SDCARD_ReadBytes(crc, sizeof(crc)) < 0) {
        SDCARD_Unselect();
        return -5;
    }

    SDCARD_Unselect();

    // first byte is VVxxxxxxxx where VV is csd.version
    if((csd[0] & 0xC0) != 0x40) // csd.version != 1
        return -6;

    uint32_t tmp = csd[7] & 0x3F; // two bits are reserved
    tmp = (tmp << 8) | csd[8];
    tmp = (tmp << 8) | csd[9];
    // Full volume: (C_SIZE+1)*512KByte == (C_SIZE+1)<<19
    // Block size: 512Byte == 1<<9
    // Blocks number: CARD_SIZE/BLOCK_SIZE = (C_SIZE+1)*(1<<19) / (1<<9) = (C_SIZE+1)*(1<<10)
    tmp = (tmp + 1) << 10;
    *num = tmp;

    return 0;
}

int SDCARD_ReadSingleBlock(uint32_t blockNum, uint8_t* buff) {
    uint8_t crc[2];

    SDCARD_Select();

    if(SDCARD_WaitNotBusy() < 0) { // keep this!
        SDCARD_Unselect();
        return -1;
    }

    /* CMD17 (SEND_SINGLE_BLOCK) command */
    uint8_t cmd[] = {
        0x40 | 0x11 /* CMD17 */,
        (blockNum >> 24) & 0xFF, /* ARG */
        (blockNum >> 16) & 0xFF,
        (blockNum >> 8) & 0xFF,
        blockNum & 0xFF,
        (0x7F << 1) | 1 /* CRC7 + end bit */
    };
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)cmd, sizeof(cmd), HAL_MAX_DELAY);

    if(SDCARD_ReadR1() != 0x00) {
        SDCARD_Unselect();
        return -2;
    }

    if(SDCARD_WaitDataToken(DATA_TOKEN_CMD17) < 0) {
        SDCARD_Unselect();
        return -3;
    }

    if(SDCARD_ReadBytes(buff, 512) < 0) {
        SDCARD_Unselect();
        return -4;
    }

    if(SDCARD_ReadBytes(crc, 2) < 0) {
        SDCARD_Unselect();
        return -5;
    }

    SDCARD_Unselect();
    return 0;
}


int SDCARD_WriteSingleBlock(uint32_t blockNum, const uint8_t* buff) {
    SDCARD_Select();

    if(SDCARD_WaitNotBusy() < 0) { // keep this!
        SDCARD_Unselect();
        return -1;
    }

    /* CMD24 (WRITE_BLOCK) command */
    uint8_t cmd[] = {
        0x40 | 0x18 /* CMD24 */,
        (blockNum >> 24) & 0xFF, /* ARG */
        (blockNum >> 16) & 0xFF,
        (blockNum >> 8) & 0xFF,
        blockNum & 0xFF,
        (0x7F << 1) | 1 /* CRC7 + end bit */
    };
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)cmd, sizeof(cmd), HAL_MAX_DELAY);

    if(SDCARD_ReadR1() != 0x00) {
        SDCARD_Unselect();
        return -2;
    }

    uint8_t dataToken = DATA_TOKEN_CMD24;
    uint8_t crc[2] = { 0xFF, 0xFF };
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, &dataToken, sizeof(dataToken), HAL_MAX_DELAY);
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)buff, 512, HAL_MAX_DELAY);
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, crc, sizeof(crc), HAL_MAX_DELAY);

    /*
        dataResp:
        xxx0abc1
            010 - Data accepted
            101 - Data rejected due to CRC error
            110 - Data rejected due to write error
    */
    uint8_t dataResp;
    SDCARD_ReadBytes(&dataResp, sizeof(dataResp));
    if((dataResp & 0x1F) != 0x05) { // data rejected
        SDCARD_Unselect();
        return -3;
    }

    if(SDCARD_WaitNotBusy() < 0) {
        SDCARD_Unselect();
        return -4;
    }

    SDCARD_Unselect();
    return 0;
}

int SDCARD_ReadBegin(uint32_t blockNum) {
    SDCARD_Select();

    if(SDCARD_WaitNotBusy() < 0) { // keep this!
        SDCARD_Unselect();
        return -1;
    }

    /* CMD18 (READ_MULTIPLE_BLOCK) command */
    uint8_t cmd[] = {
        0x40 | 0x12 /* CMD18 */,
        (blockNum >> 24) & 0xFF, /* ARG */
        (blockNum >> 16) & 0xFF,
        (blockNum >> 8) & 0xFF,
        blockNum & 0xFF,
        (0x7F << 1) | 1 /* CRC7 + end bit */
    };
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)cmd, sizeof(cmd), HAL_MAX_DELAY);

    if(SDCARD_ReadR1() != 0x00) {
        SDCARD_Unselect();
        return -2;
    }

    SDCARD_Unselect();
    return 0;
}

int SDCARD_ReadData(uint8_t* buff) {
    uint8_t crc[2];
    SDCARD_Select();

    if(SDCARD_WaitDataToken(DATA_TOKEN_CMD18) < 0) {
        SDCARD_Unselect();
        return -1;
    }

    if(SDCARD_ReadBytes(buff, 512) < 0) {
        SDCARD_Unselect();
        return -2;
    }

    if(SDCARD_ReadBytes(crc, 2) < 0) {
        SDCARD_Unselect();
        return -3;
    }

    SDCARD_Unselect();
    return 0;

}

int SDCARD_ReadEnd() {
    SDCARD_Select();

    /* CMD12 (STOP_TRANSMISSION) */
    {
        static const uint8_t cmd[] = { 0x40 | 0x0C /* CMD12 */, 0x00, 0x00, 0x00, 0x00 /* ARG */, (0x7F << 1) | 1 };
        HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)cmd, sizeof(cmd), HAL_MAX_DELAY);
    }

    /*
    The received byte immediataly following CMD12 is a stuff byte, it should be
    discarded before receive the response of the CMD12
    */
    uint8_t stuffByte;
    if(SDCARD_ReadBytes(&stuffByte, sizeof(stuffByte)) < 0) {
        SDCARD_Unselect();
        return -1;
    }

    if(SDCARD_ReadR1() != 0x00) {
        SDCARD_Unselect();
        return -2;
    }

    SDCARD_Unselect();
    return 0;
}


int SDCARD_WriteBegin(uint32_t blockNum) {
    SDCARD_Select();

    if(SDCARD_WaitNotBusy() < 0) { // keep this!
        SDCARD_Unselect();
        return -1;
    }

    /* CMD25 (WRITE_MULTIPLE_BLOCK) command */
    uint8_t cmd[] = {
        0x40 | 0x19 /* CMD25 */,
        (blockNum >> 24) & 0xFF, /* ARG */
        (blockNum >> 16) & 0xFF,
        (blockNum >> 8) & 0xFF,
        blockNum & 0xFF,
        (0x7F << 1) | 1 /* CRC7 + end bit */
    };
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)cmd, sizeof(cmd), HAL_MAX_DELAY);

    if(SDCARD_ReadR1() != 0x00) {
        SDCARD_Unselect();
        return -2;
    }

    SDCARD_Unselect();
    return 0;
}

int SDCARD_WriteData(const uint8_t* buff) {
    SDCARD_Select();

    uint8_t dataToken = DATA_TOKEN_CMD25;
    uint8_t crc[2] = { 0xFF, 0xFF };
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, &dataToken, sizeof(dataToken), HAL_MAX_DELAY);
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, (uint8_t*)buff, 512, HAL_MAX_DELAY);
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, crc, sizeof(crc), HAL_MAX_DELAY);

    /*
        dataResp:
        xxx0abc1
            010 - Data accepted
            101 - Data rejected due to CRC error
            110 - Data rejected due to write error
    */
    uint8_t dataResp;
    SDCARD_ReadBytes(&dataResp, sizeof(dataResp));
    if((dataResp & 0x1F) != 0x05) { // data rejected
        SDCARD_Unselect();
        return -1;
    }

    if(SDCARD_WaitNotBusy() < 0) {
        SDCARD_Unselect();
        return -2;
    }

    SDCARD_Unselect();
    return 0;
}

int SDCARD_WriteEnd() {
    SDCARD_Select();

    uint8_t stopTran = 0xFD; // stop transaction token for CMD25
    HAL_SPI_Transmit(&SDCARD_SPI_PORT, &stopTran, sizeof(stopTran), HAL_MAX_DELAY);

    // skip one byte before readyng "busy"
    // this is required by the spec and is necessary for some real SD-cards!
    uint8_t skipByte;
    SDCARD_ReadBytes(&skipByte, sizeof(skipByte));

    if(SDCARD_WaitNotBusy() < 0) {
        SDCARD_Unselect();
        return -1;
    }

    SDCARD_Unselect();
    return 0;
}
#endif