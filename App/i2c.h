/**
 *
 * Microvisor IoT Device Demo
 * Version 3.0.2
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _I2C_HEADER_
#define _I2C_HEADER_


#ifdef __cplusplus
extern "C" {
#endif


/*
 * CONSTANTS
 */
#define     I2C_GPIO_BANK           GPIOB


/*
 * PROTOTYPES
 */
void I2C_init(void);
void I2C_scan(void);


#ifdef __cplusplus
}
#endif


#endif  // _I2C_HEADER_
