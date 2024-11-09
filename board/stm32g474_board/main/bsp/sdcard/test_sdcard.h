/*
 * test_sdcard.h
 *
 *  Created on: Sep 1, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef BSP_SDCARD_TEST_SDCARD_H_
#define BSP_SDCARD_TEST_SDCARD_H_

#include "logger_conf.h"

#if CONFIG_USE_SDCARD

void TEST_SDCARD_Init(void);

#endif /* CONFIG_USE_SDCARD */
#endif /* BSP_SDCARD_TEST_SDCARD_H_ */
