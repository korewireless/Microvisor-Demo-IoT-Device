/**
 *
 * Microvisor IoT Device Demo
 *
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#ifndef _I2C_HEADER_
#define _I2C_HEADER_


/*
 * CONSTANTS
 */
#define     I2C_GPIO_BANK           GPIOB


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void I2C_init(void);
void I2C_scan(void);


#ifdef __cplusplus
}
#endif


#endif  // _I2C_HEADER_
