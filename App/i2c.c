/**
 *
 * Microvisor IoT Device Demo
 * Version 3.0.2
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


/*
 * STATIC PROTOTYPES
 */
static bool I2C_check(uint8_t addr);


/*
 * GLOBALS
 */
extern      I2C_HandleTypeDef   i2c;
extern      bool                use_i2c;


/**
 * @brief Initialize STM32U585 I2C1.
 */
void I2C_init(void) {
    
    // I2C1 pins are:
    //   SDA -> PB9
    //   SCL -> PB6
    i2c.Instance              = I2C1;
    i2c.Init.Timing           = 0x00C01F67;  // FROM ST SAMPLE
    i2c.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    i2c.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    i2c.Init.OwnAddress1      = 0x00;
    i2c.Init.OwnAddress2      = 0x00;
    i2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    i2c.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    i2c.Init.NoStretchMode    = I2C_NOSTRETCH_ENABLE;

    // Initialize the I2C itself with the i2c handle
    if (HAL_I2C_Init(&i2c) != HAL_OK) {
        server_error("I2C init failed");
        return;
    }

    // Check peripheral readiness
    use_i2c = I2C_check(HT16K33_I2C_ADDR);
}


/**
 * @brief Check for presence of a known device by its I2C address.
 *
 * @param addr: The device's address.
 *
 * @returns `true` if the device is present, otherwise `false`.
 */
static bool I2C_check(uint8_t addr) {
    
    uint8_t timeout_count = 0;

    while(true) {
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&i2c, addr << 1, 1, 100);
        if (status == HAL_OK) {
            return true;
        } else {
            uint32_t err = HAL_I2C_GetError(&i2c);
            server_error("HAL_I2C_IsDeviceReady() : %i", status);
            server_error("HAL_I2C_GetError():       %li", err);
        }

        // Flash the LED eight times on device not ready
        for (uint8_t i = 0 ; i < 8 ; ++i) {
            HAL_GPIO_TogglePin(LED_GPIO_BANK, LED_GPIO_PIN);
            HAL_Delay(100);
        }

        HAL_Delay(1000);
        timeout_count++;
        if (timeout_count > 10) break;
    }

    return false;
}


/**
 * @brief Scan for and list I2C devices on the bus.
 */
void I2C_scan(void) {
    
    uint8_t data = 0;
    for (uint8_t i = 2 ; i < 256 ; i += 2) {
        if (HAL_I2C_Master_Receive(&i2c, i , &data, 1, 10) == HAL_OK) {
            server_log("I2C device at %02x", i);
        }
    }
}


/**
 * @brief HAL-called function to configure I2C.
 *
 * @param i2c: A HAL I2C_HandleTypeDef pointer to the I2C instance.
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *i2c) {

    // This SDK-named function is called by HAL_I2C_Init()

    // Configure U5 peripheral clock
    RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.I2c1ClockSelection   = RCC_I2C1CLKSOURCE_PCLK1;

    // Initialize U5 peripheral clock
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        server_error("HAL_RCCEx_PeriphCLKConfig() failed");
        return;
    }

    // Enable the I2C GPIO interface clock
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure the GPIO pins for I2C
    // Pin PB6 - SCL
    // Pin PB9 - SDA
    GPIO_InitTypeDef gpioConfig = { 0 };
    gpioConfig.Pin       = GPIO_PIN_6 | GPIO_PIN_9;
    gpioConfig.Mode      = GPIO_MODE_AF_OD;
    gpioConfig.Pull      = GPIO_NOPULL;
    gpioConfig.Speed     = GPIO_SPEED_FREQ_LOW;
    gpioConfig.Alternate = GPIO_AF4_I2C1;

    // Initialize the pins with the setup data
    HAL_GPIO_Init(I2C_GPIO_BANK, &gpioConfig);

    // Enable the I2C1 clock
    __HAL_RCC_I2C1_CLK_ENABLE();
}
