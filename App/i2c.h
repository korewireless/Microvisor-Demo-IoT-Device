/**
 *
 * Microvisor IoT Device Demo
 * Version 2.1.3
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _I2C_HEADER_
#define _I2C_HEADER_


/*
 * CONSTANTS
 */
#define     I2C_GPIO_BANK           GPIOB


/*
 * PROTOTYPES
 */
void I2C_init();
void I2C_scan();


#endif  // _I2C_HEADER_
