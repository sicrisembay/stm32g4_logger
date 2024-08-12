/*!
 ******************************************************************************
 * @file           : main.c
 * @author         : Sicris Rey Embay
 * @brief          : Main program body
 ******************************************************************************
 */
#include "FreeRTOS.h"
#include "bsp/board_api.h"

int main(void)
{
	board_init();
	usb_device_init();
    vTaskStartScheduler();
}
