/*!
 ******************************************************************************
 * @file           : main.c
 * @author         : Sicris Rey Embay
 * @brief          : Main program body
 ******************************************************************************
 */
#include "FreeRTOS.h"
#include "task.h"
#include "bsp/sdcard/sdcard.h"
#include "bsp/board_api.h"
#include "cli.h"

#define MAIN_TASK_STACK_SIZE        (512)
#define MAIN_TASK_PRIORITY          (1)
static TaskHandle_t taskHandle_main = NULL;
static StaticTask_t taskStruct_main;
static StackType_t taskStackStorage[MAIN_TASK_STACK_SIZE];

static void mainTask(void * pvParam)
{
    int32_t ret;
    TickType_t xLastWakeTime;

    CLI_init();
    ret = SDCARD_Init();
    if(SDCARD_ERR_NONE != ret) {
        LPUART_printf("SDCARD_Init return %d\r\n", ret);
    }
    //usb_device_init();

    xLastWakeTime = xTaskGetTickCount();
    while(1) {
        vTaskDelayUntil(&xLastWakeTime, 1000);
    }
}

int main(void)
{
    board_init();
    taskHandle_main = xTaskCreateStatic(mainTask, "main", MAIN_TASK_STACK_SIZE,
            (void *)0, MAIN_TASK_PRIORITY, taskStackStorage, &taskStruct_main);
    configASSERT(NULL != taskHandle_main);

    vTaskStartScheduler();
}
