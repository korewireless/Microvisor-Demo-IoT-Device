/**
 *
 * Microvisor IoT Device Demo
 * Version 1.0.4
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _MAIN_H_
#define _MAIN_H_


/*
 * INCLUDES
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "cmsis_os.h"
#include "mv_syscalls.h"

// App includes
#include "logging.h"
#include "ht16k33.h"
#include "i2c.h"
#include "mcp9808.h"
#include "http.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * CONSTANTS
 */
#define     LED_GPIO_BANK           GPIOA
#define     LED_GPIO_PIN            GPIO_PIN_5
#define     LED_FLASH_PERIOD_MS     1000

#define     BUTTON_GPIO_BANK        GPIOF
#define     BUTTON_GPIO_PIN         GPIO_PIN_6

#define     DEBUG_TASK_PAUSE_MS     1000
#define     DEFAULT_TASK_PAUSE_MS   500

#define     DEBOUNCE_PERIOD_MS      20
#define     SENSOR_READ_PERIOD_MS   30000
#define     CHANNEL_KILL_PERIOD_MS  15000


/*
 * PROTOTYPES
 */
void        system_clock_config(void);
void        GPIO_init(void);
void        start_led_task(void *argument);
void        start_iot_task(void *argument);
void        log_device_info(void);


#ifdef __cplusplus
}
#endif


#endif      // _MAIN_H_
