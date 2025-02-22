/*
 * cli.c
 *
 *  Created on: Aug 18, 2024
 *      Author: Sicris Rey Embay
 */

#include "stdbool.h"
#include "stdio.h"
#include "stdarg.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"
#include "lpuart.h"
#include "cli.h"
#include "usb_device.h"

#define CLI_TASK_PRIORITY               (1)
#define CLI_TASK_STACK_SIZE             (1024)
#define CLI_COMMAND_MAX_INPUT_SIZE      (256)
#define CLI_COMMAND_MAX_OUTPUT_SIZE     (256)

#define CONFIG_CLI_STREAM_TO_USB_CDC    (1)
#define CONFIG_CLI_STREAM_TO_UART       (1)
#define DEFAULT_BLOCK_WAIT_MS           (10)
#define PRINTF_BUFFER_SIZE              (512)

typedef struct {
    SemaphoreHandle_t printfMutex;
    StaticSemaphore_t printfStruct;
    char printBuffer[PRINTF_BUFFER_SIZE];
} cli_t;

static bool bInit = false;
static TaskHandle_t taskHandle = NULL;
static StaticTask_t taskStruct = {0};
static StackType_t taskStackStorage[CLI_TASK_STACK_SIZE] = {0};
static char input_strBuf[CLI_COMMAND_MAX_INPUT_SIZE];

#define RX_STREAM_BUFFER_SIZE_BYTES     (512)
static StreamBufferHandle_t rxStreamHandle = NULL;
static uint8_t rxStreamStorage[RX_STREAM_BUFFER_SIZE_BYTES + 1] = {0};
static StaticStreamBuffer_t rxStreamStruct;

static const char * const welcomeString = "\r\n\n"
        "******************************************************\r\n"
        "* Type 'help' to view a list of registered commands.\r\n"
        "******************************************************\r\n";
static const char * const strLineSep = "\r\n";
static const char * const strPrompt = "$ ";

static cli_t cli_instance = {0};

static void CLI_Send(uint8_t * pBuf, size_t count)
{
    if((pBuf == NULL) || (count == 0)) {
        return;
    }
#if CONFIG_CLI_STREAM_TO_USB_CDC
    usb_device_cdc_transmit(pBuf, count);
#endif

#if CONFIG_CLI_STREAM_TO_UART
    LPUART_Send((uint8_t *)pBuf, count);
#endif
}


static void task_cli(void * pvParam)
{
    int16_t inputIndex = 0;
    int16_t moreDataToFollow = 0;
    char * output_str_buf = NULL;
    char rxChar = 0;

    output_str_buf = FreeRTOS_CLIGetOutputBuffer();

    /* print welcome string */
    CLI_Send((uint8_t *)welcomeString, strlen(welcomeString));
    /* print prompt symbol */
    CLI_Send((uint8_t *)strPrompt, 2);

    while(1) {
        if(xStreamBufferReceive(rxStreamHandle, &rxChar, 1, portMAX_DELAY)) {
            if((rxChar < 0x00) || (rxChar > 0x7F)) {
                continue;
            }
            if(rxChar == '\n') {
                /*
                 * A newline character was received, so the input command string is
                 * complete and can be processed.  Transmit a line separator, just to
                 * make the output easier to read.
                 */
                CLI_Send((uint8_t *)strLineSep, strlen(strLineSep));
                CLI_Send((uint8_t *)strLineSep, strlen(strLineSep));

                if(strlen(input_strBuf) == 0) {
                    /* No command to process */
                    /* Just print prompt */
                    CLI_Send((uint8_t *)strPrompt, 2);
                    inputIndex = 0;
                    continue;
                }

                /*
                 * The command interpreter is called repeatedly until it returns
                 * pdFALSE (0).
                 */
                do {
                    /*
                     * Send the command string to the command interpreter.  Any
                     * output generated by the command interpreter will be placed in the
                     * output_str_buf buffer.
                     */
                    moreDataToFollow = FreeRTOS_CLIProcessCommand
                                  (
                                      input_strBuf,                         /* The command string.*/
                                      output_str_buf,                       /* The output buffer. */
                                      CLI_COMMAND_MAX_OUTPUT_SIZE    /* The size of the output buffer. */
                                  );

                    /*
                     * Write the output generated by the command interpreter to the
                     * console.
                     */
                    CLI_Send((uint8_t *)output_str_buf, strlen(output_str_buf));

                } while( moreDataToFollow != 0 );

                /*
                 * All the strings generated by the input command have been sent.
                 * Processing of the command is complete.  Clear the input string ready
                 * to receive the next command.
                 */
                inputIndex = 0;
                memset( input_strBuf, 0x00, sizeof(input_strBuf) );

                /* print prompt symbol */
                CLI_Send((uint8_t *)strPrompt, 2);
            } else {
                /*
                 * The if() clause performs the processing after a newline character
                 * is received.  This else clause performs the processing if any other
                 * character is received. */
                if( rxChar == '\r' ) {
                    /* Ignore carriage returns. */
                } else if( rxChar == '\b' ) {
                    /*
                     * Backspace was pressed.  Erase the last character in the input
                     * buffer - if there are any
                     */
                    if( inputIndex > 0 ) {
                        inputIndex--;
                        input_strBuf[ inputIndex ] = '\0';
                    }
                } else {
                    /*
                     * A character was entered.  It was not a new line, backspace
                     * or carriage return, so it is accepted as part of the input and
                     * placed into the input buffer.  When a n is entered the complete
                     * string will be passed to the command interpreter.
                     */
                    if( inputIndex < CLI_COMMAND_MAX_INPUT_SIZE ) {
                        input_strBuf[ inputIndex ] = rxChar;
                        inputIndex++;
                    }
                }
            }
        }
    }
}


#if CONFIG_CLI_STREAM_TO_UART
void UART_receive_cb(void * object, size_t len)
{
    (void)object; // unused
    uint8_t buf[8];  // length of DMA Rx buffer
    const size_t nByte = LPUART_Receive(buf, sizeof(buf));
    CLI_Receive(buf, nByte);
}
#endif /* CONFIG_CLI_STREAM_TO_UART */


void CLI_init(void)
{
    if(bInit != true) {
#if CONFIG_CLI_STREAM_TO_UART
        LPUART_Init();
        LPUART_register_receive_cb(NULL, UART_receive_cb);
#endif
        cli_instance.printfMutex = xSemaphoreCreateRecursiveMutexStatic(&cli_instance.printfStruct);
        configASSERT(cli_instance.printfMutex != NULL);

        rxStreamHandle = xStreamBufferCreateStatic(
                                RX_STREAM_BUFFER_SIZE_BYTES,
                                1,
                                rxStreamStorage,
                                &rxStreamStruct);
        configASSERT(rxStreamHandle != NULL);

        taskHandle = xTaskCreateStatic(task_cli,
                              "cli",
                              CLI_TASK_STACK_SIZE,
                              (void *)0,
                              CLI_TASK_PRIORITY,
                              taskStackStorage,
                              &taskStruct);
        configASSERT(taskHandle != NULL);

        bInit = true;
    }
}


void CLI_Receive(uint8_t* pBuf, uint32_t len)
{
    if(bInit != true) {
        return;
    }

    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    const size_t available = xStreamBufferSpacesAvailable(rxStreamHandle);
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if(len > available) {
        len = available;
    }

    if(bInsideISR) {
        xStreamBufferSendFromISR(rxStreamHandle,
                                 pBuf,
                                 len,
                                 &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    } else {
        xStreamBufferSend(rxStreamHandle,
                          pBuf,
                          len,
                          0);
    }
}


int32_t CLI_printf(const char * format, ...)
{
    int32_t retval = PRINTF_BUFFER_SIZE;
    va_list val;
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    BaseType_t mutexGetSuccess = pdFALSE;

    if(format == NULL) {
        return CLI_ERR_INVALID_ARG;
    }

    if(bInsideISR) {
        mutexGetSuccess = xSemaphoreTakeFromISR(cli_instance.printfMutex, &higherPriorityTaskWoken);
    } else {
        mutexGetSuccess = xSemaphoreTake(cli_instance.printfMutex, DEFAULT_BLOCK_WAIT_MS / portTICK_PERIOD_MS);
    }

    if(mutexGetSuccess == pdFALSE) {
        return CLI_ERR_MUTEX;
    }

    va_start(val, format);
    int const rv = vsnprintf(cli_instance.printBuffer, PRINTF_BUFFER_SIZE - 1, format, val);
    va_end(val);

    if(bInsideISR) {
        xSemaphoreGiveFromISR(cli_instance.printfMutex, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    } else {
        xSemaphoreGive(cli_instance.printfMutex);
    }

    int32_t nBytesTransmitted = 0;
#if CONFIG_CLI_STREAM_TO_UART
    nBytesTransmitted = LPUART_Send((uint8_t *)cli_instance.printBuffer, rv);
    if(nBytesTransmitted < retval) {
        retval = nBytesTransmitted;
    }
#endif

#if CONFIG_CLI_STREAM_TO_USB_CDC
    nBytesTransmitted = usb_device_cdc_transmit((uint8_t *)cli_instance.printBuffer, rv);
    if(nBytesTransmitted < retval) {
        retval = nBytesTransmitted;
    }
#endif
    return retval;
}
