/**
 *
 * Microvisor IoT Device Demo
 *
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
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
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "cmsis_os.h"
#include "mv_syscalls.h"

// App includes
#include "logging.h"
#include "uart_logging.h"
#include "ht16k33-seg.h"
#include "i2c.h"
#include "mcp9808.h"
#include "lis3dh.h"
#include "http.h"
#include "network.h"


/*
 * CONSTANTS
 */
#define     LED_GPIO_BANK               GPIOA
#define     LED_GPIO_PIN                GPIO_PIN_5
#define     LED_FLASH_PERIOD_MS         1000

#define     LIS3DH_INT_GPIO_BANK        GPIOF
#define     LIS3DH_INT_GPIO_PIN         GPIO_PIN_3
#define     LIS3DH_INT_IRQ              EXTI3_IRQn

#define     DEBUG_TASK_PAUSE_MS         1000
#define     DEFAULT_TASK_PAUSE_MS       500

#define     DEBOUNCE_PERIOD_MS          20
#define     SENSOR_READ_PERIOD_MS       60000
#define     CHANNEL_KILL_PERIOD_MS      15000

#define     HTTP_NT_BUFFER_SIZE_R       8             // NOTE Size in records, not bytes


/*
 * ERRORS
 */
#define     ERR_CHANNEL_NOT_CLOSED                      20
#define     ERR_CHANNEL_HANDLE_NOT_ZERO                 21
#define     ERR_NOTIFICATION_CENTER_NOT_OPEN            30
#define     ERR_NOTIFICATION_CENTER_NOT_CLOSED          31
#define     ERR_NOTIFICATION_CENTER_HANDLE_NOT_ZERO     32
#define     ERR_LOG_CHANNEL_NOT_OPEN                    40
#define     ERR_LOG_CHANNEL_NOT_CLOSED                  41
#define     ERR_LOG_CHANNEL_HANDLE_NOT_ZERO             42
#define     ERR_NETWORK_NOT_OPEN                        50
#define     ERR_NETWORK_NOT_CLOSED                      51
#define     ERR_NETWORK_HANDLE_NOT_ZERO                 52
#define     ERR_NETWORK_NC_NOT_OPEN                     53
#define     ERR_TEST                                    99


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void report_and_assert(uint16_t err_code);


#ifdef __cplusplus
}
#endif


#endif      // _MAIN_H_
