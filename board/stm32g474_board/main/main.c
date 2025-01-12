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
#include "bsp/lpuart.h"
#include "cli.h"

#define MAIN_TASK_STACK_SIZE        (512)
#define MAIN_TASK_PRIORITY          (1)
static TaskHandle_t taskHandle_main = NULL;
static StaticTask_t taskStruct_main;
static StackType_t taskStackStorage[MAIN_TASK_STACK_SIZE];

static void mainTask(void * pvParam)
{
    TickType_t xLastWakeTime;

    CLI_init();
    vTaskDelay(100);
#if CONFIG_USE_SDCARD
    uint32_t retry = 0;
    int32_t ret;
    while(1) {
        ret = SDCARD_Init();
        if(SDCARD_ERR_NONE == ret) {
            LPUART_printf("SDCARD_Init OK\r\n");
            break;
        } else {
            retry++;
            if(retry < 2) {
                LPUART_printf("SDCARD_Init return %d\r\nRetrying...\r\n", ret);
                vTaskDelay(10);

            } else {
                LPUART_printf("SDCARD_Init failed!\r\n");
                vTaskDelete(taskHandle_main);
            }
        }
    }
#endif /* CONFIG_USE_SDCARD */

#if CONFIG_TEST_LFS_SD
    TEST_LFS_Init();
#endif /* CONFIG_TEST_LFS_SD */

    usb_device_init();

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
