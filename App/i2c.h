/**
 *
 * Microvisor IoT Device Demo
 * Version 1.0.3
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
bool I2C_check(uint8_t addr);
void I2C_scan();


#endif  // _I2C_HEADER_
