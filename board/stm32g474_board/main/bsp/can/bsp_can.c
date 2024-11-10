/*
 * bsp_can.c
 *
 *  Created on: Sep 8, 2024
 *      Author: Sicris Rey Embay
 */

#include "logger_conf.h"
#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bsp_can.h"
#include "test_can.h"
#include "stm32g4xx_hal.h"
#include "lpuart.h"

#define CONFIG_CAN_TASK_STACK_SIZE      (256)
#define CONFIG_CAN_TASK_PRIORITY        (1)
#define CONFIG_CAN_TX_Q_LEN             (3)
#define CONFIG_CAN_TX_ELEM_SIZE         sizeof(CAN_TX_T)
#define CONFIG_CAN_RX_Q_LEN             (3)
#define CONFIG_CAN_RX_ELEM_SIZE         sizeof(CAN_RX_T)

#define CAN_TX_BIT                      (0x01UL)
#define CAN_RX_BIT                      (0x02UL)

static char const * const taskName[CONFIG_CAN_COUNT] = {
#if (CONFIG_CAN_COUNT >= 1)
    "can1",
#endif
#if (CONFIG_CAN_COUNT >= 2)
    "can2",
#endif
#if (CONFIG_CAN_COUNT >= 3)
    "can3",
#endif
};


typedef struct {
    uint32_t prescaler;
    uint32_t sjw;
    uint32_t tseg1;
    uint32_t tseg2;
} TIMING_CONFIG_T;


typedef struct {
    FDCAN_HandleTypeDef FDCAN_handle;
    IRQn_Type IRQn;
    ARBIT_BITRATE_T arbit_bps;
    DATA_BITRATE_T data_bps;
    TaskHandle_t task;
    StaticTask_t taskStruct;
    QueueHandle_t txQueueHandle;
    StaticQueue_t txQueueStruct;
    QueueHandle_t rxQueueHandle;
    StaticQueue_t rxQueueStruct;
    uint32_t debugRxCount;
    bool txInProgress;
    bool isEnabled;
} CAN_T;

static bool bInit = false;
static CAN_T can[N_CAN_ID];
static StackType_t taskStack[N_CAN_ID][CONFIG_CAN_TASK_STACK_SIZE];
static uint8_t txQueueSto[N_CAN_ID][CONFIG_CAN_TX_Q_LEN * CONFIG_CAN_TX_ELEM_SIZE];
static uint8_t rxQueueSto[N_CAN_ID][CONFIG_CAN_RX_Q_LEN * CONFIG_CAN_RX_ELEM_SIZE];

static const uint32_t const DLC_TO_BYTES[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12,
                    16, 20, 24, 32, 48, 64};

/*
 *
 * Refer to https://kvaser.com/support/calculators/can-fd-bit-timing-calculator/
 *
 * APB1 Peripheral clock : 80MHz (PCLK1)
 * FDCAN clock input     : 80MHz (set to PCLK1)
 * FDCAN periph clock    : 80MHz (80MHz with prescaler of DIV1)
 * Clock Tolerance       : 4687.5ppm (see datasheet)
 * Node delay            : 180ns (see transceiver worst delay)
 */
static TIMING_CONFIG_T const DEFAULT_ARBITRATION_TIMING[N_ARBIT_BITRATE] = {
    {
        /* 500kHz, 85.63% Sample Point */
        .prescaler = 1,
        .sjw = 23,
        .tseg1 = 136,
        .tseg2 = 23
    },
    {
        /* 1MHz, 85% Sample Point */
        .prescaler = 1,
        .sjw = 12,
        .tseg1 = 67,
        .tseg2 = 12
    }
};


static TIMING_CONFIG_T const DEFAULT_DATA_TIMING[N_DATA_BITRATE] = {
    {
        /* 500kHz, 85.63% Sample Point */
        .prescaler = 1,
        .sjw = 23,
        .tseg1 = 136,
        .tseg2 = 23
    },
    {
        /* 1MHz, 85% Sample Point */
        .prescaler = 1,
        .sjw = 12,
        .tseg1 = 67,
        .tseg2 = 12
    }
};


static FDCAN_GlobalTypeDef * DEFAULT_FDCAN[N_CAN_ID] = {
#if (CONFIG_CAN_COUNT >=1)
    FDCAN1,
#endif
#if (CONFIG_CAN_COUNT >=2)
    FDCAN2,
#endif
#if (CONFIG_CAN_COUNT >=3)
    FDCAN3,
#endif
};


static IRQn_Type const DEFAULT_FCAN_IRQ[N_CAN_ID] = {
#if (CONFIG_CAN_COUNT >=1)
    FDCAN1_IT0_IRQn,
#endif
#if (CONFIG_CAN_COUNT >=2)
    FDCAN2_IT0_IRQn,
#endif
#if (CONFIG_CAN_COUNT >=3)
    FDCAN3_IT0_IRQn,
#endif
};


static uint32_t const DEFAULT_FRAME_FORMAT[CONFIG_CAN_COUNT] = {
#if (CONFIG_CAN_COUNT >= 1)
#if CONFIG_FD_CAN_ONE
#if CONFIG_BRS_CAN_ONE
    FDCAN_FRAME_FD_BRS,
#else
    FDCAN_FRAME_FD_NO_BRS,
#endif /* CONFIG_BRS_CAN_ONE */
#else
    FDCAN_FRAME_CLASSIC,
#endif /* CONFIG_FD_CAN_ONE */
#endif /* (CONFIG_CAN_COUNT >= 1) */

#if (CONFIG_CAN_COUNT >= 2)
#if CONFIG_FD_CAN_TWO
#if CONFIG_BRS_CAN_TWO
    FDCAN_FRAME_FD_BRS,
#else
    FDCAN_FRAME_FD_NO_BRS,
#endif /* CONFIG_BRS_CAN_TWO */
#else
    FDCAN_FRAME_CLASSIC,
#endif /* CONFIG_FD_CAN_TWO */
#endif /* (CONFIG_CAN_COUNT >= 2) */

#if (CONFIG_CAN_COUNT >= 3)
#if CONFIG_FD_CAN_THREE
#if CONFIG_BRS_CAN_THREE
    FDCAN_FRAME_FD_BRS,
#else
    FDCAN_FRAME_FD_NO_BRS,
#endif /* CONFIG_BRS_CAN_THREE */
#else
    FDCAN_FRAME_CLASSIC,
#endif /* CONFIG_FD_CAN_THREE */
#endif /* (CONFIG_CAN_COUNT >= 3) */
};


static DATA_BITRATE_T const DEFAULT_DATA_BITRATE[CONFIG_CAN_COUNT] = {
#if (CONFIG_CAN_COUNT >= 1)
    (DATA_BITRATE_T)CONFIG_DATA_BPS_CAN_ONE,
#endif
#if (CONFIG_CAN_COUNT >= 2)
    (DATA_BITRATE_T)CONFIG_DATA_BPS_CAN_TWO,
#endif
#if (CONFIG_CAN_COUNT >= 3)
    (DATA_BITRATE_T)CONFIG_DATA_BPS_CAN_THREE,
#endif
};


static ARBIT_BITRATE_T const DEFAULT_ARBIT_BITRATE[CONFIG_CAN_COUNT] = {
#if (CONFIG_CAN_COUNT >= 1)
    (DATA_BITRATE_T)CONFIG_ARBIT_BPS_CAN_ONE,
#endif
#if (CONFIG_CAN_COUNT >= 2)
    (DATA_BITRATE_T)CONFIG_ARBIT_BPS_CAN_TWO,
#endif
#if (CONFIG_CAN_COUNT >= 3)
    (DATA_BITRATE_T)CONFIG_ARBIT_BPS_CAN_THREE,
#endif
};


static void can_task(void * pvParam)
{
    CAN_RX_T rxElem;
    CAN_TX_T txElem;
    CAN_T * const me = (CAN_T *)pvParam;
    uint32_t notifyValue = 0;

    /* Configure Interrupt Priority */
    NVIC_SetPriority(me->IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    HAL_NVIC_EnableIRQ(me->IRQn);

    me->txInProgress = false;
    me->debugRxCount = 0;

    while(1) {
        if(pdPASS == xTaskNotifyWait(
                                pdFALSE,
                                UINT32_MAX,
                                &notifyValue,
                                portMAX_DELAY)) {
            if(0 != (notifyValue & CAN_TX_BIT)) {
                if(pdTRUE == xQueueReceive(me->txQueueHandle, &txElem, 0)) {
                    if(HAL_OK == HAL_FDCAN_AddMessageToTxFifoQ(
                                        &(me->FDCAN_handle),
                                        &(txElem.header),
                                        &(txElem.data[0]))) {
                        me->txInProgress = true;
                    }
                } else {
                    me->txInProgress = false;
                }
            }

            if(0 != (notifyValue & CAN_RX_BIT)) {
                while(pdTRUE == xQueueReceive(me->rxQueueHandle,
                        &rxElem, 0)) {
                    me->debugRxCount++;
                    CAN_LOG_DEBUG("id: 0x%03lx dlc: %02d\r\n",
                            rxElem.header.Identifier,
                            DLC_TO_BYTES[rxElem.header.DataLength]);
                    /*
                     * [0]     : tag (0xFF)
                     * [1..2]  : length
                     * [3..6]  : timestamp offset
                     * [7..10] : SEQ number
                     * [11]    : packet type
                     *             0x00: Start Time in Ticks
                     *             0x01: Tx CAN standard
                     *             0x02: Rx CAN standard
                     *             0x03: Tx CAN-FD
                     *             0x04: Rx CAN-FD
                     * [12..N] : payload
                     * [N]     : checksum8
                     */
                    /// TODO: Write to SD card
                }
            }
        }
    }
    vTaskDelete(NULL);
}


/*
 * NOTE: This called from the interrupt
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    CAN_RX_T rxElem = {0};
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    CAN_T * me = NULL;
    uint32_t id = 0;

    for(id = 0; id < CONFIG_CAN_COUNT; id++) {
        me = &can[id];
        if(me->FDCAN_handle.Instance == DEFAULT_FDCAN[id]) {
            break;
        }
    }

    if(id >= CONFIG_CAN_COUNT) {
        // invalid
        return;
    }

    if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        /* Retrieve Rx messages from RX FIFO0 */
        if(HAL_OK == HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &(rxElem.header), &(rxElem.data[0]))) {
            if(pdTRUE == xQueueSendFromISR(me->rxQueueHandle, &rxElem, &xHigherPriorityTaskWoken)) {
                me->debugRxCount++;
                xTaskNotifyFromISR(me->task, CAN_RX_BIT, eSetBits, &xHigherPriorityTaskWoken);
            } else {
                /// TODO: notify that CAN Rx Queue has overrun!!!
                /// ASSERT for now
                configASSERT(pdFALSE);
            }
        } else {
            /// TODO: Handle this
            configASSERT(pdFALSE);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/*
 * NOTE: This called from the interrupt
 */
void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hfdcan)
{
    CAN_T * me = NULL;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t id = 0;

    for(id = 0; id < CONFIG_CAN_COUNT; id++) {
        me = &can[id];
        if(me->FDCAN_handle.Instance == DEFAULT_FDCAN[id]) {
            break;
        }
    }

    if(id >= CONFIG_CAN_COUNT) {
        // invalid
        return;
    }

    xTaskNotifyFromISR(me->task, CAN_TX_BIT, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void BSP_CAN_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    if(bInit == true) {
        return;
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
    configASSERT(HAL_OK == HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit));
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

#if (CONFIG_CAN_COUNT >= 1)
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif
#if (CONFIG_CAN_COUNT >= 2)
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif
#if (CONFIG_CAN_COUNT >= 3)
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_FDCAN3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_FDCAN3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif

    for(uint32_t i = 0; i < N_CAN_ID; i++) {
        CAN_T * const me = &can[i];
        me->isEnabled = false;
        me->IRQn = DEFAULT_FCAN_IRQ[i];

        me->txQueueHandle = xQueueCreateStatic(
                                CONFIG_CAN_TX_Q_LEN,
                                CONFIG_CAN_TX_ELEM_SIZE,
                                &txQueueSto[i][0],
                                &me->txQueueStruct);
        configASSERT(NULL != me->txQueueHandle);
        me->rxQueueHandle = xQueueCreateStatic(
                                CONFIG_CAN_RX_Q_LEN,
                                CONFIG_CAN_RX_ELEM_SIZE,
                                &rxQueueSto[i][0],
                                &me->rxQueueStruct);
        configASSERT(NULL != me->rxQueueHandle);

        me->FDCAN_handle.Instance = DEFAULT_FDCAN[i];
        me->FDCAN_handle.Init.ClockDivider = FDCAN_CLOCK_DIV1;
        me->FDCAN_handle.Init.FrameFormat = DEFAULT_FRAME_FORMAT[i];
        me->FDCAN_handle.Init.Mode = FDCAN_MODE_NORMAL;
        me->FDCAN_handle.Init.AutoRetransmission = DISABLE;
        me->FDCAN_handle.Init.TransmitPause = DISABLE;
        me->FDCAN_handle.Init.ProtocolException = DISABLE;
        me->data_bps = DEFAULT_DATA_BITRATE[i];
        me->FDCAN_handle.Init.DataPrescaler = DEFAULT_DATA_TIMING[me->data_bps].prescaler;
        me->FDCAN_handle.Init.DataSyncJumpWidth = DEFAULT_DATA_TIMING[me->data_bps].sjw;
        me->FDCAN_handle.Init.DataTimeSeg1 = DEFAULT_DATA_TIMING[me->data_bps].tseg1;
        me->FDCAN_handle.Init.DataTimeSeg2 = DEFAULT_DATA_TIMING[me->data_bps].tseg2;
        if(DEFAULT_FRAME_FORMAT[i] == FDCAN_FRAME_CLASSIC) {
            me->arbit_bps = DEFAULT_ARBIT_BITRATE[i];  // Set the same as Data Bit Rate
        } else if(DEFAULT_FRAME_FORMAT[i] == FDCAN_FRAME_FD_NO_BRS) {
            me->arbit_bps = DEFAULT_DATA_BITRATE[i];  // Set the same as Data Bit Rate
        } else if(DEFAULT_FRAME_FORMAT[i] == FDCAN_FRAME_FD_BRS) {
            me->arbit_bps = DEFAULT_ARBIT_BITRATE[i];
        } else {
            configASSERT(false);  // This should not happen.
        }
        me->FDCAN_handle.Init.NominalPrescaler = DEFAULT_ARBITRATION_TIMING[me->arbit_bps].prescaler;
        me->FDCAN_handle.Init.NominalSyncJumpWidth = DEFAULT_ARBITRATION_TIMING[me->arbit_bps].sjw;
        me->FDCAN_handle.Init.NominalTimeSeg1 = DEFAULT_ARBITRATION_TIMING[me->arbit_bps].tseg1;
        me->FDCAN_handle.Init.NominalTimeSeg2 = DEFAULT_ARBITRATION_TIMING[me->arbit_bps].tseg2;
        me->FDCAN_handle.Init.StdFiltersNbr = 0;
        me->FDCAN_handle.Init.ExtFiltersNbr = 0;
        me->FDCAN_handle.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
        configASSERT(HAL_OK == HAL_FDCAN_Init(&me->FDCAN_handle));

        me->task = xTaskCreateStatic(
                            can_task,
                            taskName[i],
                            CONFIG_CAN_TASK_STACK_SIZE,
                            (void *)me,
                            CONFIG_CAN_TASK_PRIORITY,
                            &(taskStack[i][0]),
                            &(me->taskStruct));
        configASSERT(me->task != NULL);
    }

    /*
     * CLI Test
     */
    TEST_CAN_init();

    bInit = true;
}

bool BSP_CAN_configure(const CAN_ID_T id,
                       const ARBIT_BITRATE_T arbit_bps,
                       const DATA_BITRATE_T data_bps)
{
    if((id >= N_CAN_ID) || (arbit_bps >= N_ARBIT_BITRATE) ||
       (data_bps >= N_DATA_BITRATE)) {
        return false;
    }

    CAN_T * const me = &(can[id]);
    TIMING_CONFIG_T const * const pArbBitTiming = &(DEFAULT_ARBITRATION_TIMING[arbit_bps]);
    TIMING_CONFIG_T const * const pDataTiming = &(DEFAULT_DATA_TIMING[data_bps]);

    if(HAL_FDCAN_STATE_READY != HAL_FDCAN_GetState(&(me->FDCAN_handle))) {
        return false;
    }

    me->FDCAN_handle.Init.NominalPrescaler = pArbBitTiming->prescaler;
    me->FDCAN_handle.Init.NominalSyncJumpWidth = pArbBitTiming->sjw;
    me->FDCAN_handle.Init.NominalTimeSeg1 = pArbBitTiming->tseg1;
    me->FDCAN_handle.Init.NominalTimeSeg2 = pArbBitTiming->tseg2;
    me->FDCAN_handle.Init.DataPrescaler = pDataTiming->prescaler;
    me->FDCAN_handle.Init.DataSyncJumpWidth = pDataTiming->sjw;
    me->FDCAN_handle.Init.DataTimeSeg1 = pDataTiming->tseg1;
    me->FDCAN_handle.Init.DataTimeSeg2 = pDataTiming->tseg2;
    if(HAL_OK != HAL_FDCAN_Init(&(me->FDCAN_handle))) {
        return false;
    }
    me->arbit_bps = arbit_bps;
    me->data_bps = data_bps;

    return true;
}


bool BSP_CAN_is_enabled(const CAN_ID_T id)
{
    if(id >= N_CAN_ID) {
        return false;
    }

    CAN_T * const me = &(can[id]);
    return me->isEnabled;
}


bool BSP_CAN_start(const CAN_ID_T id)
{
    if(id >= N_CAN_ID) {
        return false;
    }

    CAN_T * const me = &(can[id]);

    if(!me->isEnabled) {
        if(HAL_OK != HAL_FDCAN_Start(&(me->FDCAN_handle))) {
            CAN_LOG_DEBUG("HAL_FDCAN_Start error!\r\n");
            return false;
        }

        if(HAL_OK != HAL_FDCAN_ActivateNotification(
                            &(me->FDCAN_handle),
                            FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_TX_FIFO_EMPTY,
                            FDCAN_TX_BUFFER0)) {
            CAN_LOG_DEBUG("HAL_FDCAN_ActivateNotification error!\r\n");
            return false;
        }

        NVIC_EnableIRQ(me->IRQn);
        me->isEnabled = true;
    }

    CAN_LOG_INFO("CAN%d enabled\r\n", (id + 1));

    return true;
}


bool BSP_CAN_stop(const CAN_ID_T id)
{
    if(id >= N_CAN_ID) {
        return false;
    }

    CAN_T * const me = &(can[id]);

    if(me->isEnabled) {
        me->isEnabled = false;
        NVIC_DisableIRQ(me->IRQn);

        if(HAL_OK != HAL_FDCAN_DeactivateNotification(
                        &(me->FDCAN_handle),
                        FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) {
            CAN_LOG_DEBUG("HAL_FDCAN_DeactivateNotification error!\r\n");
            return false;
        }

        if(HAL_OK != HAL_FDCAN_Stop(&(me->FDCAN_handle))) {
            CAN_LOG_DEBUG("HAL_FDCAN_Stop error!\r\n");
            return false;
        }
    }
    CAN_LOG_INFO("CAN%d disabled\r\n", (id + 1));
    return true;
}


bool BSP_CAN_send(const CAN_ID_T id, CAN_TX_T * pElem)
{
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if((id >= N_CAN_ID) || (pElem == NULL)) {
        return false;
    }

    CAN_T * const me = &(can[id]);

    if(me->isEnabled != true) {
        /* Not yet initialized or disabled */
        return false;
    }

    if(bInsideISR) {
        if(pdTRUE != xQueueSendFromISR(me->txQueueHandle, pElem, &higherPriorityTaskWoken)) {
            return false;
        }
        if(me->txInProgress == false) {
            xTaskNotifyFromISR(me->task, CAN_TX_BIT, eSetBits, &higherPriorityTaskWoken);
        }
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    } else {
        if(pdTRUE != xQueueSend(me->txQueueHandle, pElem, 0)) {
            return false;
        }
        if(me->txInProgress == false) {
            xTaskNotify(me->task, CAN_TX_BIT, eSetBits);
        }
    }

    return true;
}

// CAN-FD
void FDCAN1_IT0_IRQHandler(void)
{
#if (CONFIG_CAN_COUNT >= 1)
    CAN_T * const me = &can[CAN_ONE];
    HAL_FDCAN_IRQHandler(&me->FDCAN_handle);
#endif
}


void FDCAN2_IT0_IRQHandler(void)
{
#if (CONFIG_CAN_COUNT >= 2)
    CAN_T * const me = &can[CAN_TWO];
    HAL_FDCAN_IRQHandler(&me->FDCAN_handle);
#endif
}

void FDCAN3_IT0_IRQHandler(void)
{
#if (CONFIG_CAN_COUNT >= 3)
    CAN_T * const me = &can[CAN_THREE];
    HAL_FDCAN_IRQHandler(&me->FDCAN_handle);
#endif
}
