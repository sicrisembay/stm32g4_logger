/*!
 ******************************************************************************
 * @file           : main.c
 * @author         : Sicris Rey Embay
 * @brief          : Main program body
 ******************************************************************************
 */
#include "FreeRTOS.h"
#include "bsp/board_api.h"
#include "cli.h"

int main(void)
{
    board_init();
    CLI_init();
//    usb_device_init();
    vTaskStartScheduler();
}
