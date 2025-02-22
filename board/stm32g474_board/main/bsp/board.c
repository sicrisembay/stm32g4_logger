#include "logger_conf.h"
#include "stdbool.h"
#include "stdint.h"
#include "stm32g4xx_hal.h"
#include "board_api.h"
#include "test_board.h"
#include "lpuart.h"
#include "spi/bsp_spi.h"
#include "can/bsp_can.h"

static PCD_HandleTypeDef hpcd_USB_FS;

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}


static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /*
     *  Configure the main internal regulator output voltage
     */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

#if CONFIG_CORE_FREQ_160MHZ
    /*
     *  Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     *
     * HSI RC = 16MHz
     * SYSCLK = 160MHz
     *
     * Enable HSI48 for USB
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN = 20;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        __disable_irq();
        while(1);
    }
#else
#error "Invalid Core Frequency!"
#endif

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4; // SPI source 40MHz

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        __disable_irq();
        while(1);
    }
}


static void BoardGpio_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /*
     * LED indicators
     */
    /* Green LED */
    HAL_GPIO_WritePin(GREEN_LED_Port, GREEN_LED_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = GREEN_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GREEN_LED_Port, &GPIO_InitStruct);
    /* Red LED */
    HAL_GPIO_WritePin(RED_LED_Port, RED_LED_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = RED_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RED_LED_Port, &GPIO_InitStruct);

    /*
     * SD Card
     */
    /* Chip Select */
    HAL_GPIO_WritePin(SD_CS_Port, SD_CS_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = SD_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(SD_CS_Port, &GPIO_InitStruct);
    /* SD Card Detect Input */
    GPIO_InitStruct.Pin = SD_DETECT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SD_DETECT_Port, &GPIO_InitStruct);
}


static void MX_USB_PCD_Init(void)
{
    hpcd_USB_FS.Instance = USB;
    hpcd_USB_FS.Init.dev_endpoints = 8;
    hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_FS.Init.Sof_enable = DISABLE;
    hpcd_USB_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_FS.Init.lpm_enable = DISABLE;
    hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
    if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK) {
        __disable_irq();
        while(1);
    }
}

void HAL_PCD_MspInit(PCD_HandleTypeDef* hpcd)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    if(hpcd->Instance==USB) {
        /*
         * Initializes the peripherals clocks
         */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
        PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
            __disable_irq();
            while(1);
        }

        /* Peripheral clock enable */
        __HAL_RCC_USB_CLK_ENABLE();
    }
}


void board_init()
{
    HAL_Init();
    SystemClock_Config();
    SysTick->CTRL &= ~1U;   // Explicitly disable systick to prevent its ISR runs before scheduler start
    BoardGpio_Config();
    LPUART_Init();
    BSP_SPI_init();
    MX_USB_PCD_Init();
    BSP_CAN_init();

    TEST_BOARD_Init();

    NVIC_SetPriority(SysTick_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(FDCAN1_IT0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(USB_HP_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(USB_LP_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(USBWakeUp_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
}


void board_led_write(bool isOn)
{
    if(isOn) {
        HAL_GPIO_WritePin(GREEN_LED_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GREEN_LED_Port, GREEN_LED_Pin, GPIO_PIN_SET);
    }

}
