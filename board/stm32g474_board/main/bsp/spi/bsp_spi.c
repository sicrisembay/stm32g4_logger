/*
 * bsp_spi.c
 *
 *  Created on: Aug 20, 2024
 *      Author: Sicris Rey Embay
 */
#include "stdbool.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_spi.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_dma.h"
#include "bsp_spi.h"
#include "test_spi.h"

#define SPI_MANAGER_TASK_PRIORITY       (1)
#define SPI_MANAGER_TASK_STACK_SIZE     (512)
#define SPI_TRANSACT_QUEUE_LEN          (16)
#define SPI_MANAGER_DEFAULT_TIMEOUT     (100)
#define SPI_MANAGER_TRANSFER_COMPLETE   (0x01UL)
#define SPI_MANAGER_TRANSFER_ERROR      (0x02UL)

static uint32_t const DEFAULT_SPI_BAUDRATEPRESCALER[N_BSP_SPI_CLK] = {
    LL_SPI_BAUDRATEPRESCALER_DIV2,
    LL_SPI_BAUDRATEPRESCALER_DIV4,
    LL_SPI_BAUDRATEPRESCALER_DIV8,
    LL_SPI_BAUDRATEPRESCALER_DIV16,
    LL_SPI_BAUDRATEPRESCALER_DIV32,
    LL_SPI_BAUDRATEPRESCALER_DIV64,
    LL_SPI_BAUDRATEPRESCALER_DIV128,
    LL_SPI_BAUDRATEPRESCALER_DIV256,
};

typedef struct {
    void * pTxBuf;
    void * pRxBuf;
    size_t len;
    uint32_t br;
    SPI_MODE_T mode;
    SPI_ChipSelect cs;
    SemaphoreHandle_t semRequestor;
    int32_t * pStatus;
} SPI_TRANSACTION_T;

static bool bInit = false;
static TaskHandle_t taskHandle_SpiManager = NULL;
static StaticTask_t taskStruct_SpiManager;
static StackType_t taskStackStorage[SPI_MANAGER_TASK_STACK_SIZE];
static SemaphoreHandle_t semHandle_SpiManager = NULL;   // <-- TODO: Use TaskNotify mechanism instead
static StaticSemaphore_t semStruct_SpiManager;          // <-- TODO: Use TaskNotify mechanism instead
static QueueHandle_t queueHandle_transact = NULL;
static StaticQueue_t queueStruct_transact;
static uint8_t queueStorage[SPI_TRANSACT_QUEUE_LEN * sizeof(SPI_TRANSACTION_T)];

void DMA1_Channel3_IRQHandler(void)
{
    /* SPI Tx DMA */
    if(LL_DMA_IsActiveFlag_TC3(DMA1)) {
        LL_DMA_ClearFlag_TC3(DMA1);
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    } else if(LL_DMA_IsActiveFlag_TE3(DMA1)) {
        LL_DMA_ClearFlag_TE3(DMA1);
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    }
}


void DMA1_Channel4_IRQHandler(void)
{
    BaseType_t taskWoken = pdFALSE;

    /* SPI Rx DMA */
    if(LL_DMA_IsActiveFlag_TC4(DMA1)) {
        LL_DMA_ClearFlag_TC4(DMA1);
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
        xTaskNotifyFromISR(taskHandle_SpiManager, SPI_MANAGER_TRANSFER_COMPLETE, eSetBits, &taskWoken);
    } else if(LL_DMA_IsActiveFlag_TE4(DMA1)) {
        LL_DMA_ClearFlag_TE4(DMA1);
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
        xTaskNotifyFromISR(taskHandle_SpiManager, SPI_MANAGER_TRANSFER_ERROR, eSetBits, &taskWoken);
    }
    portYIELD_FROM_ISR(taskWoken);
}


static void SPI_ManagerTask(void * pvParam)
{
    SPI_TRANSACTION_T entry;
    uint32_t notifyValue;

    while(bInit != true) {
        vTaskDelay(1);
    }

    /* Configure Interrupt Priority */
    NVIC_SetPriority(DMA1_Channel3_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(DMA1_Channel4_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    /* Enable Interrupt */
    NVIC_EnableIRQ(DMA1_Channel3_IRQn);;
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);;

    while(1) {
        if(pdTRUE == xQueueReceive(queueHandle_transact, &entry, portMAX_DELAY)) {
            /* Check validity */
            if((entry.len == 0) || (entry.pTxBuf == NULL) ||
               (entry.pRxBuf == NULL) || (entry.br >= N_BSP_SPI_CLK)) {
                /* Invalid */
                if(entry.pStatus != NULL) {
                    *(entry.pStatus) = SPI_ERR_INVALID_ARG;
                }
                if(entry.semRequestor != NULL) {
                    xSemaphoreGive(entry.semRequestor);
                }
                continue;
            }

//            LL_SPI_Disable(SPI1);
            /*
             * Update SPI clock
             */
            LL_SPI_SetBaudRatePrescaler(SPI1, DEFAULT_SPI_BAUDRATEPRESCALER[entry.br]);
            /*
             * Update transaction Mode
             */
            switch(entry.mode) {
                case SPI_MODE0: {
                    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_LOW);
                    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_1EDGE);
                    break;
                }
                case SPI_MODE1: {
                    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_LOW);
                    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_2EDGE);
                    break;
                }
                case SPI_MODE2: {
                    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_HIGH);
                    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_1EDGE);
                    break;
                }
                case SPI_MODE3: {
                    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_HIGH);
                    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_2EDGE);
                    break;
                }
                default: {
                    /* Default to SPI_MODE0 */
                    LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_LOW);
                    LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_1EDGE);
                    break;
                }
            }
            LL_SPI_Enable(SPI1);
            /*
             * Updated transaction data width
             */
            /// Data Width is fixed for now at 8 bits.
            /*
             * Initialize DMA for this transaction
             */
            // Tx DMA
            LL_DMA_ConfigTransfer(DMA1,
                    LL_DMA_CHANNEL_3,
                    LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_PRIORITY_HIGH |
                    LL_DMA_MODE_NORMAL | LL_DMA_PERIPH_NOINCREMENT |
                    LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_BYTE |
                    LL_DMA_MDATAALIGN_BYTE
                    );
            LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_3,
                    (uint32_t)(entry.pTxBuf),
                    LL_SPI_DMA_GetRegAddr(SPI1),
                    LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
            LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, entry.len);
            LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_3, LL_DMAMUX_REQ_SPI1_TX);
            // Rx DMA
            LL_DMA_ConfigTransfer(DMA1,
                    LL_DMA_CHANNEL_4,
                    LL_DMA_DIRECTION_PERIPH_TO_MEMORY | LL_DMA_PRIORITY_HIGH |
                    LL_DMA_MODE_NORMAL | LL_DMA_PERIPH_NOINCREMENT |
                    LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_BYTE |
                    LL_DMA_MDATAALIGN_BYTE
                    );
            LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_4,
                    LL_SPI_DMA_GetRegAddr(SPI1),
                    (uint32_t)(entry.pRxBuf),
                    LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
            LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, entry.len);
            LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_4, LL_DMAMUX_REQ_SPI1_RX);
            /*
             * Clear previous notification, if any
             */
            xTaskNotifyStateClear(NULL);

            /*
             * Chip Select
             */
            if(entry.cs != NULL) {
                entry.cs(true);
            }
            /*
             * Start DMA
             */
            LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_3);
            LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_4);
            LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_3);
            LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_4);
            LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_QUARTER);
            LL_SPI_EnableDMAReq_TX(SPI1);
            LL_SPI_EnableDMAReq_RX(SPI1);
            LL_SPI_Enable(SPI1);
            LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);
            LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
            /*
             * Wait for DMA complete transfer or Error
             */
            if(pdFALSE == xTaskNotifyWait(0, ULONG_MAX, &notifyValue, SPI_MANAGER_DEFAULT_TIMEOUT)) {
                /* Timed out */
                if(entry.pStatus != NULL) {
                    *(entry.pStatus) = SPI_ERR_TIMEOUT;
                }
                if(entry.semRequestor != NULL) {
                    xSemaphoreGive(entry.semRequestor);
                }
                if(entry.cs != NULL) {
                    entry.cs(false);
                }
                continue;
            }
            /*
             * Chip De-Select
             */
            if(entry.cs != NULL) {
                entry.cs(false);
            }
            /*
             * Post to requester
             */
            if((notifyValue & SPI_MANAGER_TRANSFER_COMPLETE) != 0) {
                /* Success */
                if(entry.pStatus != NULL) {
                    *(entry.pStatus) = SPI_ERR_NONE;
                }
                if(entry.semRequestor != NULL) {
                    xSemaphoreGive(entry.semRequestor);
                }
            }

            if((notifyValue & SPI_MANAGER_TRANSFER_ERROR) != 0) {
                /* DMA Error */
                if(entry.pStatus != NULL) {
                    *(entry.pStatus) = SPI_ERR_DMA_TRANSFER_ERROR;
                }
                if(entry.semRequestor != NULL) {
                    xSemaphoreGive(entry.semRequestor);
                }
            }
        }
    }
}

void BSP_SPI_init(void)
{
    if(bInit == true) {
        // already initialized
        return;
    }
    LL_SPI_InitTypeDef SPI_InitStruct = {0};
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMAMUX1);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    /*
     * SPI1 GPIO Configuration
     */
    /* PA5   ------> SPI1_SCK */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_5;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    /* PA6   ------> SPI1_MISO */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    /* PA7   ------> SPI1_MOSI */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*
     * SPI Configuration
     */
    /* SPI1 parameter configuration */
    SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
    SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
    SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
    SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
//    SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16;
    SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV128;
    SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
    SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_InitStruct.CRCPoly = 7;
    configASSERT(SUCCESS == LL_SPI_Init(SPI1, &SPI_InitStruct));
    LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);
    /*
     * SPI Manager Task
     */
    taskHandle_SpiManager = xTaskCreateStatic(SPI_ManagerTask,
                                    "SPI-Manager",
                                    SPI_MANAGER_TASK_STACK_SIZE,
                                    (void *)0,
                                    SPI_MANAGER_TASK_PRIORITY,
                                    taskStackStorage,
                                    &taskStruct_SpiManager);
    configASSERT(taskHandle_SpiManager != NULL);
    /*
     * SPI Semaphore
     */
    semHandle_SpiManager = xSemaphoreCreateBinaryStatic(&semStruct_SpiManager);
    configASSERT(semHandle_SpiManager != NULL);
    /*
     * Transaction Request Queue
     */
    queueHandle_transact = xQueueCreateStatic(SPI_TRANSACT_QUEUE_LEN,
                                sizeof(SPI_TRANSACTION_T),
                                &queueStorage[0],
                                &queueStruct_transact);
    configASSERT(queueHandle_transact != NULL);
    /*
     * CLI Test
     */
    TEST_SPI_init();

    bInit = true;
}

int32_t BSP_SPI_transact(void * pTxBuf,
                      void * pRxBuf,
                      size_t length,
                      SPI_MODE_T mode,
                      SPI_ChipSelect cs,
                      BSP_SPI_CLK_T clk,
                      SemaphoreHandle_t semRequester,
                      int32_t * pStatus)
{
    SPI_TRANSACTION_T entry = {0};
    bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());

    if((pTxBuf == NULL) || (pRxBuf == NULL) ||
       (length == 0) || (clk >= N_BSP_SPI_CLK)) {
        return SPI_ERR_INVALID_ARG;
    }
    entry.pTxBuf = pTxBuf;
    entry.pRxBuf = pRxBuf;
    entry.len = length;
    entry.mode = mode;
    entry.cs = cs;
    entry.br = (uint32_t)clk;
    entry.semRequestor = semRequester;
    entry.pStatus = pStatus;
    if(bInsideISR) {
        BaseType_t taskWoken = pdFALSE;
        if(pdPASS == xQueueSendFromISR(queueHandle_transact, &entry, &taskWoken)) {
            portYIELD_FROM_ISR(taskWoken);
        } else {
            return SPI_ERR_Q_FULL;
        }
    } else {
        if(pdPASS != xQueueSend(queueHandle_transact, &entry, 0)) {
            return SPI_ERR_Q_FULL;
        }
    }
    return SPI_ERR_NONE;
}
