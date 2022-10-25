/**
 *
 * Microvisor IoT Device Demo
 * Version 2.1.3
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


extern I2C_HandleTypeDef i2c;

// Data
uint8_t _local_mode = LIS3DH_MODE_NORMAL;
uint8_t _local_range = 0;


/**
    @brief  Check the device is connected and operational.

    @returns `true` if we can read values and they are right,
             otherwise `false`.
 */
bool LIS3DH_init() {
    uint8_t did_value = LIS3DH_get_device_id();
    server_log("LIS3DH Device ID: 0x%04x", did_value);

    if (did_value == 0x33) {
        LIS3DH_get_range();
        server_log("LIS3DH Range: %i", _local_range);
        return true;
    }

    return false;
}


/**
 * @brief Enable the ADC. The ADC sampling frequency is the same as that of the ODR
 *        in LISxDH_CTRL_REG1. The input range is 0.8-1.6v and the
 *         data output is expressed in 2's complement left-aligned. The resolution is
 *         8-bit in low-power mode 10 bits otherwise.
 *
 *  @param state: ADC should be enabled (`true`) or disabled (`false`).
 */
void LIS3DH_enable_ADC(bool state) {
    _set_reg_bit(LIS3DH_TEMP_CFG_REG, 7, state ? 1 : 0);
    _set_reg_bit(LIS3DH_CTRL_REG4,    7, state ? 1 : 0);
}


/**
 * @brief This method returns the current value of the ADC line. You must pass one of
 *        the lines (1, 2, or 3) or you will get an invalid return value. You must also
 *        have enabled the ADC by calling LIS3DH_enable_ADC(true)
 *
 * @param adc_line: The required ADC line.
 *
 * @returns The ADC value.
 */
double LIS3DH_read_ADC(uint8_t adc_line) {
    uint8_t reg = (adc_line << 1) + 6;
    uint8_t read[2] = {0};
    _get_multi_reg(reg, read, 2);

    // Shift and sign extend
    double val = (double)((((read[0] >> 6) | (read[1] << 2)) << 22) >> 22);
    val /= 512.0; // In low-power mode, there will be lower resolution

    // Affine transformation to map to appropriate voltage
    return (val * 0.4) + 1.2;
}


/**
 * @brief Set the state of the accelerometer axes.
 *
 * LIS3DH_CTRL_REG1 enables/disables accelerometer axes
 *   bit 0 = X axis
 *   bit 1 = Y axis
 *   bit 2 = Z axis
 *
 * @param state: Should the acclerometer be enabled (`true`) or
 *               disabled (`false`).
 */
void LIS3DH_enable_accel(bool enable) {
    uint8_t val = _get_reg(LIS3DH_CTRL_REG1);
    if (enable) {
        val |= 0x07;
    } else {
        val &= 0xF8;
    }
    _set_reg(LIS3DH_CTRL_REG1, val);
}


/**
 * @brief Read data from the Accelerometer.
 *
 * @param result: A pointer to an AccelResult struct.
 */
void LIS3DH_get_accel(AccelResult* result) {
    uint8_t reading[6] = {0};
    _get_multi_reg(LIS3DH_OUT_X_L_INCR, reading, 6);

    // Read and sign extend the raw data
    result->x = ((reading[0] | (reading[1] << 8)) << 16) >> 16;
    result->y = ((reading[2] | (reading[3] << 8)) << 16) >> 16;
    result->z = ((reading[4] | (reading[5] << 8)) << 16) >> 16;

    // Multiply by full-scale range to return in G
    result->x = (result->x / 32000.0) * _local_range;
    result->y = (result->y / 32000.0) * _local_range;
    result->z = (result->z / 32000.0) * _local_range;
}


/**
 *  @brief  Det the current full-scale range (Gs) of the accelerometer.
 *
 *  @returns The current range.
 */
uint8_t LIS3DH_get_range() {
    uint8_t range_bits = (_get_reg(LIS3DH_CTRL_REG4) & 0x30) >> 4;
    if (range_bits == 0x00) {
        _local_range = 2;
    } else if (range_bits == 0x01) {
        _local_range = 4;
    } else if (range_bits == 0x02) {
        _local_range = 8;
    } else {
        _local_range = 16;
    }
    return _local_range;
}


/**
 *  @brief Set the full-scale range of the accelerometer.
 *
 *  Supported ranges are ±2, ±4, ±8 and ±16G.
 *
 *  @returns The current range.
 */
uint8_t LIS3DH_set_range(uint8_t rangeA) {
    uint8_t val = _get_reg(LIS3DH_CTRL_REG4) & 0xCF;
    uint8_t range_bits = 0;
    if (rangeA <= 2) {
        range_bits = 0x00;
        _local_range = 2;
    } else if (rangeA <= 4) {
        range_bits = 0x01;
        _local_range = 4;
    } else if (rangeA <= 8) {
        range_bits = 0x02;
        _local_range = 8;
    } else {
        range_bits = 0x03;
        _local_range = 16;
    }

    _set_reg(LIS3DH_CTRL_REG4, val | (range_bits << 4));
    return _local_range;
}


/**
 *  @brief Set accelerometer data rate in Hz.
 *
 *  Supported data rates are 0 (Shutdown), 1, 10, 25, 50, 100, 200, 400,
 *  1250 (Normal Mode only), 1600 (Low-Power Mode only), and 5000 (Low-Power Mode only).
 *  The requested data rate will be rounded up to the closest supported rate
 *  and the actual data rate will be returned.
 *
 *  @returns The current range.
 */
uint32_t LIS3DH_set_data_rate(uint32_t rate) {
    uint8_t val = _get_reg(LIS3DH_CTRL_REG1) & 0x0F;
    bool normal_mode = (val < 8);
    if (rate == 0) {
        // 0b0000 -> power-down mode
        // we've already ANDed-out the top 4 bits; just write back
        rate = 0;
    } else if (rate <= 1) {
        val |= 0x10;
        rate = 1;
    } else if (rate <= 10) {
        val |= 0x20;
        rate = 10;
    } else if (rate <= 25) {
        val |= 0x30;
        rate = 25;
    } else if (rate <= 50) {
        val |= 0x40;
        rate = 50;
    } else if (rate <= 100) {
        val |= 0x50;
        rate = 100;
    } else if (rate <= 200) {
        val |= 0x60;
        rate = 200;
    } else if (rate <= 400) {
        val |= 0x70;
        rate = 400;
    } else if (normal_mode) {
        val |= 0x90;
        rate = 1250;
    } else if (rate <= 1600) {
        val |= 0x80;
        rate = 1600;
    } else {
        val |= 0x90;
        rate = 5000;
    }

    _set_reg(LIS3DH_CTRL_REG1, val);
    return rate;
}


/**
 * @brief Set the mode of the accelerometer by passing a constant (LIS3DH_MODE_NORMAL,
 *        IS3DH_MODE_LOW_POWER, LIS3DH_MODE_HIGH_RESOLUTION).
 *
 * @param mode: The required LISD3H mode.
 */
void LIS3DH_set_mode(uint8_t mode) {
    _set_reg_bit(LIS3DH_CTRL_REG1, 3, mode & 0x01);
    _set_reg_bit(LIS3DH_CTRL_REG4, 3, mode & 0x02);
    _local_mode = mode;
}


void LIS3DH_configure_high_pass_filter(uint8_t filters, uint8_t cutoff, uint8_t mode) {
    // clear and set filters
    filters = LIS3DH_HPF_DISABLED | filters;
    _set_reg(LIS3DH_CTRL_REG2, filters | cutoff | mode);
}


/**
 * @brief Enable/disable and configure FIFO buffer.
 *
 * @param state: Enable the FIFO (`true`) or disable it (`false`).
 */
void LIS3DH_configure_fifo(bool enable, uint8_t fifo_mode) {
    // Enable/disable the FIFO
    _set_reg_bit(LIS3DH_CTRL_REG5, 6, enable ? 1 : 0);

    // NOTE FIFO Trigger selection (LIS3DH_FIFO_CTRL_REG bit 5) defaults to int1.
    //      This library currently doesn't change this bit, so trigger selection
    //      is always set to trigger on int1
    uint8_t val = _get_reg(LIS3DH_FIFO_CTRL_REG) & 0x3F;

    if (enable) {
        _set_reg(LIS3DH_FIFO_CTRL_REG, (val | fifo_mode));
    } else {
        // Set mode to bypass
        _set_reg(LIS3DH_FIFO_CTRL_REG, val);
    }
}


/**
 * @brief Configure the LIS3DH to assert the INT pin on clicks.
 *
 * @param enable:     Enable click interrupts (`true`) or disable them (`false`).
 * @param click_type: The type of click to monitor — see lis3dh.h.
 * @param threshold:  Inertial interrupts threshold in float Gs.
 * @param time_limit: Period to check for clicks in integer milliseconds.
 * @param latency:    Laterncy period in integer milliseconds.
 * @param window:     Measurement window in integer milliseconds.
 */
void LIS3DH_configure_click_irq(bool enable, uint8_t click_type, float threshold, uint8_t time_limit, uint8_t latency, uint8_t window) {

    // Set the enable / disable flag
    _set_reg_bit(LIS3DH_CTRL_REG3, 7, enable ? 1 : 0);
    _set_reg_bit(LIS3DH_CTRL_REG3, 6, enable ? 1 : 0);

    // If the click interrupt is not disabled, clear LIS3DH_CLICK_CFG register and return
    if (!enable) {
        _set_reg(LIS3DH_CLICK_CFG, 0x00);
        return;
    }

    // Set the LIS3DH_CLICK_CFG register
    _set_reg(LIS3DH_CLICK_CFG, click_type);

    // Set the LIS3DH_CLICK_THS register
    uint8_t latched_bit = _get_reg(LIS3DH_CLICK_THS) & 0x80;    // Get LIR_Click bit
    if (threshold < 0) threshold *= -1.0;                       // Make sure we have a positive value
    if (threshold > _local_range) threshold = (float)_local_range;     // Make sure it doesn't exceed the _range

    uint32_t ithreshold = (uint32_t)((threshold / (float)_local_range) * 127);
    _set_reg(LIS3DH_CLICK_THS, latched_bit | (ithreshold & 0x7F));

    // Set the LIS3DH_TIME_LIMIT register (max time for a click)
    _set_reg(LIS3DH_TIME_LIMIT, time_limit);
    // Set the LIS3DH_TIME_LATENCY register (min time between clicks for double click)
    _set_reg(LIS3DH_TIME_LATENCY, latency);
    // Set the LIS3DH_TIME_WINDOW register (max time for double click)
    _set_reg(LIS3DH_TIME_WINDOW, window);
}


/**
 * @brief Configure the LIS3DH to assert the INT pin on falls.
 *
 * @param enable:     Latch (`true`) or unlatch (`false`) the interrupts.
 * @param threshold:  Inertial interrupts threshold in float Gs.
 * @param time_limit: Period to check for falls in integer milliseconds.
 * @param latency:    Laterncy period in integer milliseconds.
 * @param window:     Measurement window in integer milliseconds.
 */
void LIS3DH_configure_free_fall_irq(bool enable, float threshold, uint8_t duration) {
    LIS3DH_configure_inertial_irq(enable, threshold, duration, LIS3DH_AOI | LIS3DH_X_LOW | LIS3DH_Y_LOW | LIS3DH_Z_LOW);
}


/**
 * @brief Configure the LIS3DH to assert the INT pin on movement.
 *
 * @param enable:    Latch (`true`) or unlatch (`false`) the interrupts.
 * @param threshold: Inertial interrupts threshold in float Gs.
 * @param duration:  Period to check for motion in integer milliseconds.
 * @param options:   Bitfield indicated when an interrupt will be triggered.
 */
void LIS3DH_configure_inertial_irq(bool enable, float threshold, uint8_t duration, uint8_t options) {
    // Set the enable flag
    _set_reg_bit(LIS3DH_CTRL_REG3, 6, enable ? 1 : 0);

    // If we're disabling the interrupt, don't set anything else
    if (!enable) return;

    // Clamp the threshold
    if (threshold < 0) threshold = threshold * -1.0;    // Make sure we have a positive value
    if (threshold > _local_range) threshold = (float)_local_range;        // Make sure it doesn't exceed the _range

    // Set the threshold
    uint32_t ithreshold = (uint32_t)((threshold / (float)_local_range) * 127);
    _set_reg(LIS3DH_INT1_THS, (ithreshold & 0x7F));

    // Set the duration
    _set_reg(LIS3DH_INT1_DURATION, duration & 0x7F);

    // Set the options flags
    _set_reg(LIS3DH_INT1_CFG, options);
}


/**
 * @brief Latch the LIS3dH's interrupts.
 *        This locks interrupt values until read by calling `LIS3DH_get_interrupt_table()`.
 *
 * @param enable: Latch (`true`) or unlatch (`false`) the interrupts.
 */
void LIS3DH_configure_irq_latching(bool enable) {
    _set_reg_bit(LIS3DH_CTRL_REG5, 3, enable ? 1 : 0);
    _set_reg_bit(LIS3DH_CLICK_THS, 7, enable ? 1 : 0);
}


/**
 * @brief Get the LIS3dH's interrupt table.
 *
 * @param data: Pointer to an InterruptTable structure.
 *              (see lis3dh.h)
 */
void LIS3DH_get_interrupt_table(InterruptTable* data) {
    uint8_t int_1 = _get_reg(LIS3DH_INT1_SRC);
    uint8_t click = _get_reg(LIS3DH_CLICK_SRC);
    data->int_1 = (int_1 & 0x40) != 0;
    data->x_low = (int_1 & 0x01) != 0;
    data->x_high = (int_1 & 0x02) != 0;
    data->y_low = (int_1 & 0x04) != 0;
    data->y_high = (int_1 & 0x08) != 0;
    data->z_low = (int_1 & 0x10) != 0;
    data->z_high = (int_1 & 0x20) != 0;
    data->click = (click & 0x40) != 0;
    data->single_click = (click & 0x10) != 0;
    data->double_click = (click & 0x20) != 0;
}


/**
 * @brief Get the LIS3dH's FIFO status.
 *
 * @param data: Pointer to a FifoState structure.
 *              (see lis3dh.h)
 */
void LIS3DH_get_fifo_stats(FifoState* data) {
    uint8_t stats = _get_reg(LIS3DH_FIFO_SRC_REG);
    data->watermark = (stats & 0x80) != 0;
    data->overrun = (stats & 0x40) != 0;
    data->empty = (stats & 0x20) != 0;
    data->unread = (stats & 0x1F) + ((stats & 0x40) ? 1 : 0);
}


/**
 * @brief Set default values for registers.
 */
void LIS3DH_reset() {
    _set_reg(LIS3DH_CTRL_REG1, 0x07);
    _set_reg(LIS3DH_CTRL_REG2, 0x00);
    _set_reg(LIS3DH_CTRL_REG3, 0x00);
    _set_reg(LIS3DH_CTRL_REG4, 0x00); // Sets range to default
    _set_reg(LIS3DH_CTRL_REG5, 0x00);
    _set_reg(LIS3DH_CTRL_REG6, 0x00);
    _set_reg(LIS3DH_INT1_CFG, 0x00);
    _set_reg(LIS3DH_INT1_THS, 0x00);
    _set_reg(LIS3DH_INT1_DURATION, 0x00);
    _set_reg(LIS3DH_CLICK_CFG, 0x00);
    _set_reg(LIS3DH_CLICK_THS, 0x00);
    _set_reg(LIS3DH_TIME_LIMIT, 0x00);
    _set_reg(LIS3DH_TIME_LATENCY, 0x00);
    _set_reg(LIS3DH_TIME_WINDOW, 0x00);
    _set_reg(LIS3DH_FIFO_CTRL_REG, 0x00);
    _set_reg(LIS3DH_TEMP_CFG_REG, 0x00);

    // Reads the default range from register and + sets local _local_range property
    LIS3DH_get_range();
}


/**
 * @brief Read and return the Device ID (0x33).
 */
uint8_t LIS3DH_get_device_id() {
    return _get_reg(LIS3DH_WHO_AM_I);
}


/********************** PRIVATE METHODS *********************/

void _set_reg(uint8_t reg, uint8_t val) {
    uint8_t send_data[2] = {0};
    send_data[0] = reg;
    send_data[1] = val;
    HAL_I2C_Master_Transmit(&i2c, LIS3DH_ADDR << 1, send_data, 2, 100);
}

void _set_reg_bit(uint8_t reg, uint8_t bit, bool state) {
    uint8_t val = _get_reg(reg);
    
    if (state) {
        val |= (0x01 << bit);
    } else {
        val &= ~(0x01 << bit);
    }
    
    _set_reg(reg, val);
}

uint8_t _get_reg(uint8_t reg) {
    uint8_t result = 0;
    HAL_I2C_Master_Transmit(&i2c, LIS3DH_ADDR << 1, &reg,    1, 100);
    HAL_I2C_Master_Receive(&i2c,  LIS3DH_ADDR << 1, &result, 1, 100);
    return result;
}

void _get_multi_reg(uint8_t reg, uint8_t* result, uint8_t num_bytes) {
    HAL_I2C_Master_Transmit(&i2c, LIS3DH_ADDR << 1, &reg,   1,         100);
    HAL_I2C_Master_Receive(&i2c,  LIS3DH_ADDR << 1, result, num_bytes, 100);
}
