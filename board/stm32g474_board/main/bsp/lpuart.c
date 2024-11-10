/*
 * lpuart.c
 *
 *  Created on: Aug 17, 2024
 *      Author: Sicris
 */
#include "stdbool.h"
#include "stdio.h"
#include "stdarg.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "lpuart.h"

#define DEFAULT_BLOCK_WAIT_MS         (10)

#define TX_DATA_AVAILABLE             (0x000000001UL)
#define TX_COMPLETE                   (0x000000002UL)

#define UART_TASK_PRIORITY            (1)
#define UART_TASK_STACK_SIZE          (512)

#define TX_STREAM_BUFFER_SIZE_BYTES   (512)
#define RX_STREAM_BUFFER_SIZE_BYTES   (256)
#define RX_DMA_BUFFER_SIZE            (8)
#define PRINTF_BUFFER_SIZE            (128)

typedef struct {
    UART_HandleTypeDef handle;
    DMA_HandleTypeDef txDMA;
    DMA_HandleTypeDef rxDMA;
    TaskHandle_t taskHandle;
    StaticTask_t taskStruct;
    StackType_t taskStackStorage[UART_TASK_STACK_SIZE];
    StreamBufferHandle_t txStreamHandle;
    uint8_t txStreamStorage[TX_STREAM_BUFFER_SIZE_BYTES + 1];
    StaticStreamBuffer_t txStreamStruct;
    SemaphoreHandle_t txWriterMutex;
    StaticSemaphore_t txWriterStruct;
    StreamBufferHandle_t rxStreamHandle;
    uint8_t rxStreamStorage[RX_STREAM_BUFFER_SIZE_BYTES + 1];
    StaticStreamBuffer_t rxStreamStruct;
    SemaphoreHandle_t rxReaderMutex;
    StaticSemaphore_t rxReaderStruct;
    uart_tx_state_t txState;
    uint8_t rxDmaBuffer[RX_DMA_BUFFER_SIZE];
    char printBuffer[PRINTF_BUFFER_SIZE];
    SemaphoreHandle_t printfMutex;
    StaticSemaphore_t printfStruct;
} uart_t;

static bool bInit = false;
static uart_t uart;

void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&uart.rxDMA);
}


void DMA1_Channel2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&uart.txDMA);
}


void LPUART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart.handle);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    configASSERT(huart == &uart.handle);
    xTaskNotifyFromISR(uart.taskHandle, TX_COMPLETE, eSetBits, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    static uint32_t old_pos = 0;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    size_t available = xStreamBufferSpacesAvailable(uart.txStreamHandle);
    size_t nBytesInDmaBuffer;

    if(size != old_pos) {
        if(size > old_pos) {
            nBytesInDmaBuffer = size - old_pos;
            if(nBytesInDmaBuffer > available) {
                /* Do not overwrite stream buffer */
                nBytesInDmaBuffer = available;
            }
            xStreamBufferSendFromISR(uart.rxStreamHandle,
                                     &(uart.rxDmaBuffer[old_pos]),
                                     nBytesInDmaBuffer,
                                     &higherPriorityTaskWoken);
        } else {
            nBytesInDmaBuffer = RX_DMA_BUFFER_SIZE - old_pos + size;
            size_t nByteToWrite = RX_DMA_BUFFER_SIZE - old_pos;
            if(nByteToWrite > available) {
                nByteToWrite = available;
            }
            xStreamBufferSendFromISR(uart.rxStreamHandle,
                                     &(uart.rxDmaBuffer[old_pos]),
                                     nByteToWrite,
                                     &higherPriorityTaskWoken);
            available -= nByteToWrite;
            nByteToWrite = size;
            if(nByteToWrite > 0) {
                if(nByteToWrite > available) {
                    nByteToWrite = available;
                }
                xStreamBufferSendFromISR(uart.rxStreamHandle,
                                         &(uart.rxDmaBuffer[0]),
                                         nByteToWrite,
                                         &higherPriorityTaskWoken);
                available -= nByteToWrite;
            }
        }
    }

    old_pos = size;
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void uart_task(void * pxParam)
{
    uart_t * const me = (uart_t *)pxParam;
    uint32_t notifyValue = 0;
    uint8_t txBufBlock[64];

    /* Configure Interrupt Priority */
    NVIC_SetPriority(DMA1_Channel1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(DMA1_Channel2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(LPUART1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    /* Enable Interrupt */
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    HAL_NVIC_EnableIRQ(LPUART1_IRQn);

    configASSERT(HAL_OK == HAL_UARTEx_ReceiveToIdle_DMA(&me->handle, me->rxDmaBuffer, RX_DMA_BUFFER_SIZE));

    while(1) {
        if(pdPASS == xTaskNotifyWait(
                             pdFALSE,
                             UINT32_MAX,
                             &notifyValue,
                             portMAX_DELAY)) {
            if(TX_DATA_AVAILABLE & notifyValue) {
                if(me->txState == UART_TX_STATE_IDLE) {
                    if(pdTRUE == xSemaphoreTake(me->txWriterMutex, portMAX_DELAY)) {
                        size_t count = xStreamBufferBytesAvailable(me->txStreamHandle);
                        if(count > sizeof(txBufBlock)) {
                            count = sizeof(txBufBlock);
                        }
                        if(count > 0) {
                            count = xStreamBufferReceive(me->txStreamHandle,
                                                       txBufBlock,
                                                       count,
                                                       portMAX_DELAY);
                            HAL_UART_Transmit_DMA(&me->handle, txBufBlock, count);
                            me->txState = UART_TX_STATE_BUSY;
                        }
                        xSemaphoreGive(me->txWriterMutex);
                    }
                }
            }
            if(TX_COMPLETE & notifyValue) {
                if(pdTRUE == xSemaphoreTake(me->txWriterMutex, portMAX_DELAY)) {
                    size_t count = xStreamBufferBytesAvailable(me->txStreamHandle);
                    if(count == 0) {
                        /* Nothing to transmit */
                        me->txState = UART_TX_STATE_IDLE;
                    } else {
                        if(count > sizeof(txBufBlock)) {
                            count = sizeof(txBufBlock);
                        }
                        count = xStreamBufferReceive(me->txStreamHandle,
                                                   txBufBlock,
                                                   count,
                                                   portMAX_DELAY);
                        HAL_UART_Transmit_DMA(&me->handle, txBufBlock, count);
                    }
                    xSemaphoreGive(me->txWriterMutex);
                }
            }
        }
    }
}


void LPUART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    if(bInit != true) {
        /* Clock enable */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
        PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
        configASSERT(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) == HAL_OK);
        __HAL_RCC_LPUART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMAMUX1_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();
        /* Configure IO */
        GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF12_LPUART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        /* Configure LPUART */
        uart.txState = UART_TX_STATE_IDLE;
        uart.handle.Instance = LPUART1;
        uart.handle.Init.BaudRate = 115200;
        uart.handle.Init.WordLength = UART_WORDLENGTH_8B;
        uart.handle.Init.StopBits = UART_STOPBITS_1;
        uart.handle.Init.Parity = UART_PARITY_NONE;
        uart.handle.Init.Mode = UART_MODE_TX_RX;
        uart.handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        uart.handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
        uart.handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
        uart.handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
        configASSERT(HAL_OK == HAL_UART_Init(&uart.handle));
        configASSERT(HAL_OK == HAL_UARTEx_DisableFifoMode(&uart.handle));

        /* Configure DMA for Rx */
        uart.rxDMA.Instance = DMA1_Channel1;
        uart.rxDMA.Init.Request = DMA_REQUEST_LPUART1_RX;
        uart.rxDMA.Init.Direction = DMA_PERIPH_TO_MEMORY;
        uart.rxDMA.Init.PeriphInc = DMA_PINC_DISABLE;
        uart.rxDMA.Init.MemInc = DMA_MINC_ENABLE;
        uart.rxDMA.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        uart.rxDMA.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        uart.rxDMA.Init.Mode = DMA_CIRCULAR;
        uart.rxDMA.Init.Priority = DMA_PRIORITY_LOW;
        configASSERT(HAL_DMA_Init(&uart.rxDMA) == HAL_OK);
        __HAL_LINKDMA(&uart.handle, hdmarx, uart.rxDMA);
        /* Configure DMA for Tx */
        uart.txDMA.Instance = DMA1_Channel2;
        uart.txDMA.Init.Request = DMA_REQUEST_LPUART1_TX;
        uart.txDMA.Init.Direction = DMA_MEMORY_TO_PERIPH;
        uart.txDMA.Init.PeriphInc = DMA_PINC_DISABLE;
        uart.txDMA.Init.MemInc = DMA_MINC_ENABLE;
        uart.txDMA.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        uart.txDMA.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        uart.txDMA.Init.Mode = DMA_NORMAL;
        uart.txDMA.Init.Priority = DMA_PRIORITY_LOW;
        configASSERT(HAL_DMA_Init(&uart.txDMA) == HAL_OK);
        __HAL_LINKDMA(&uart.handle, hdmatx, uart.txDMA);
        /* Create FreeRTOS task */
        uart.taskHandle = xTaskCreateStatic(uart_task,
                                    "uart",
                                    UART_TASK_STACK_SIZE,
                                    (void *)&uart,
                                    UART_TASK_PRIORITY,
                                    uart.taskStackStorage,
                                    &uart.taskStruct);
        configASSERT(uart.taskHandle != NULL);
        /* Create stream */
        uart.txStreamHandle = xStreamBufferCreateStatic(TX_STREAM_BUFFER_SIZE_BYTES,
                                             1,
                                             uart.txStreamStorage,
                                             &uart.txStreamStruct);
        configASSERT(uart.txStreamHandle != NULL);
        uart.rxStreamHandle = xStreamBufferCreateStatic(RX_STREAM_BUFFER_SIZE_BYTES,
                                             1,
                                             uart.rxStreamStorage,
                                             &uart.rxStreamStruct);
        configASSERT(uart.rxStreamHandle != NULL);
        /* Create mutex */
        uart.txWriterMutex = xSemaphoreCreateMutexStatic(&uart.txWriterStruct);
        configASSERT(uart.txWriterMutex != NULL);
        uart.rxReaderMutex = xSemaphoreCreateMutexStatic(&uart.rxReaderStruct);
        configASSERT(uart.rxReaderMutex != NULL);
        uart.printfMutex = xSemaphoreCreateRecursiveMutexStatic(&uart.printfStruct);
        configASSERT(uart.printfMutex != NULL);
        bInit = true;
    }
}


int32_t LPUART_Send(const uint8_t * buf, const uint32_t len)
{
    int32_t ret = 0;
    bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    BaseType_t mutexGetSuccess = pdFALSE;
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if(bInit != true) {
        return LPUART_ERR_INVALID_STATE;
    }

    if(NULL == buf) {
        ret = LPUART_ERR_INVALID_ARG;
    } else {
        /** Get mutex to protect resource ****************************************/
        if(bInsideISR) {
            mutexGetSuccess = xSemaphoreTakeFromISR(uart.txWriterMutex, &higherPriorityTaskWoken);
        } else {
            mutexGetSuccess = xSemaphoreTake(uart.txWriterMutex, DEFAULT_BLOCK_WAIT_MS / portTICK_PERIOD_MS);
        }

        if(mutexGetSuccess == pdFALSE) {
            ret = LPUART_ERR_MUTEX;
        } else {
            size_t available = xStreamBufferSpacesAvailable(uart.txStreamHandle);
            if(len > available) {
                ret = LPUART_ERR_SPACE_INSUFFICIENT;
            } else {
                if(bInsideISR) {
                    ret = xStreamBufferSendFromISR(uart.txStreamHandle,
                                    buf,
                                    len,
                                    &higherPriorityTaskWoken);
                    xTaskNotifyFromISR(uart.taskHandle, TX_DATA_AVAILABLE, eSetBits, &higherPriorityTaskWoken);
                } else {
                    ret = xStreamBufferSend(uart.txStreamHandle,
                                    buf,
                                    len,
                                    DEFAULT_BLOCK_WAIT_MS / portTICK_PERIOD_MS);
                    xTaskNotify(uart.taskHandle, TX_DATA_AVAILABLE, eSetBits);
                }
            }
            /** Release protected resource *******************************************/
            if(bInsideISR) {
                xSemaphoreGiveFromISR(uart.txWriterMutex, &higherPriorityTaskWoken);
                portYIELD_FROM_ISR(higherPriorityTaskWoken);
            } else {
                xSemaphoreGive(uart.txWriterMutex);
            }
        }
    }

    return ret;
}


bool LPUART_TxDone(void)
{
    return ((uart.txState == UART_TX_STATE_IDLE) &&
            (xStreamBufferIsEmpty(uart.txStreamHandle)));
}


int32_t LPUART_Receive(uint8_t * buf, const uint32_t len)
{
    int32_t ret = 0;
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    BaseType_t mutexGetSuccess = pdFALSE;

    if(NULL == buf) {
        return LPUART_ERR_INVALID_ARG;
    }

    if(bInsideISR) {
        mutexGetSuccess = xSemaphoreTakeFromISR(uart.rxReaderMutex, &higherPriorityTaskWoken);
    } else {
        mutexGetSuccess = xSemaphoreTake(uart.rxReaderMutex, DEFAULT_BLOCK_WAIT_MS / portTICK_PERIOD_MS);
    }

    if(mutexGetSuccess == pdFALSE) {
        return LPUART_ERR_MUTEX;
    }

    const size_t available = xStreamBufferBytesAvailable(uart.rxStreamHandle);
    const size_t nReadCount = (len > available) ? available : len;
    ret = nReadCount;
    if(bInsideISR) {
        xStreamBufferReceiveFromISR(uart.rxStreamHandle,
                                buf,
                                nReadCount,
                                &higherPriorityTaskWoken);
        xSemaphoreGiveFromISR(uart.rxReaderMutex, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    } else {
        xStreamBufferReceive(uart.rxStreamHandle,
                            buf,
                            nReadCount,
                            DEFAULT_BLOCK_WAIT_MS / portTICK_PERIOD_MS);
        xSemaphoreGive(uart.rxReaderMutex);
    }

    return ret;
}


int32_t LPUART_printf(const char * format, ...)
{
    va_list val;
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    BaseType_t mutexGetSuccess = pdFALSE;

    if(format == NULL) {
        return LPUART_ERR_INVALID_ARG;
    }

    if(bInsideISR) {
        mutexGetSuccess = xSemaphoreTakeFromISR(uart.rxReaderMutex, &higherPriorityTaskWoken);
    } else {
        mutexGetSuccess = xSemaphoreTake(uart.rxReaderMutex, DEFAULT_BLOCK_WAIT_MS / portTICK_PERIOD_MS);
    }

    if(mutexGetSuccess == pdFALSE) {
        return LPUART_ERR_MUTEX;
    }

    va_start(val, format);
    int const rv = vsnprintf(uart.printBuffer, PRINTF_BUFFER_SIZE - 1, format, val);
    va_end(val);

    if(bInsideISR) {
        xSemaphoreGiveFromISR(uart.rxReaderMutex, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    } else {
        xSemaphoreGive(uart.rxReaderMutex);
    }

    return(LPUART_Send((uint8_t *)uart.printBuffer, rv));
}

