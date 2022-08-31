/**
 *
 * Microvisor IoT Device Demo
 * Version 2.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _LIS3DH_HEADER_
#define _LIS3DH_HEADER_


// https://github.com/electricimp/LIS3DH/blob/master/LIS3DH.device.lib.nut

/*
 *  CONSTANTS
 */
// Sensor I2C address
#define LIS3DH_ADDR                         0x19

// Register addresses
#define LIS3DH_TEMP_CFG_REG                 0x1F // Enable temp/ADC
#define LIS3DH_CTRL_REG1                    0x20 // Data rate, normal/low power mode, enable xyz axis
#define LIS3DH_CTRL_REG2                    0x21 // HPF config
#define LIS3DH_CTRL_REG3                    0x22 // Int1 interrupt type enable/disable
#define LIS3DH_CTRL_REG4                    0x23 // BDU, endian data sel, range, high res mode, self test, SPI 3 or 4 wire
#define LIS3DH_CTRL_REG5                    0x24 // boot, FIFO enable, latch int1 & int2, 4D enable int1 & int2 with 6D bit set
#define LIS3DH_CTRL_REG6                    0x25 // int2 interrupt settings, set polarity of int1 and int2 pins
#define LIS3DH_OUT_X_L_INCR                 0xA8
#define LIS3DH_OUT_X_L                      0x28
#define LIS3DH_OUT_X_H                      0x29
#define LIS3DH_OUT_Y_L                      0x2A
#define LIS3DH_OUT_Y_H                      0x2B
#define LIS3DH_OUT_Z_L                      0x2C
#define LIS3DH_OUT_Z_H                      0x2D
#define LIS3DH_FIFO_CTRL_REG                0x2E
#define LIS3DH_FIFO_SRC_REG                 0x2F
#define LIS3DH_INT1_CFG                     0x30
#define LIS3DH_INT1_SRC                     0x31
#define LIS3DH_INT1_THS                     0x32
#define LIS3DH_INT1_DURATION                0x33
#define LIS3DH_CLICK_CFG                    0x38
#define LIS3DH_CLICK_SRC                    0x39
#define LIS3DH_CLICK_THS                    0x3A
#define LIS3DH_TIME_LIMIT                   0x3B
#define LIS3DH_TIME_LATENCY                 0x3C
#define LIS3DH_TIME_WINDOW                  0x3D
#define LIS3DH_WHO_AM_I                     0x0F

// Bitfield values
#define LIS3DH_X_LOW                        0x01
#define LIS3DH_X_HIGH                       0x02
#define LIS3DH_Y_LOW                        0x04
#define LIS3DH_Y_HIGH                       0x08
#define LIS3DH_Z_LOW                        0x10
#define LIS3DH_Z_HIGH                       0x20
#define LIS3DH_SIX_D                        0x40
#define LIS3DH_AOI                          0x80

// High Pass Filter values
#define LIS3DH_HPF_DISABLED                 0x00
#define LIS3DH_HPF_AOI_INT1                 0x01
#define LIS3DH_HPF_AOI_INT2                 0x02
#define LIS3DH_HPF_CLICK                    0x04
#define LIS3DH_HPF_FDS                      0x08

#define LIS3DH_HPF_CUTOFF1                  0x00
#define LIS3DH_HPF_CUTOFF2                  0x10
#define LIS3DH_HPF_CUTOFF3                  0x20
#define LIS3DH_HPF_CUTOFF4                  0x30

#define LIS3DH_HPF_DEFAULT_MODE             0x00
#define LIS3DH_HPF_REFERENCE_SIGNAL         0x40
#define LIS3DH_HPF_NORMAL_MODE              0x80
#define LIS3DH_HPF_AUTORESET_ON_INTERRUPT   0xC0

#define LIS3DH_FIFO_BYPASS_MODE             0x00
#define LIS3DH_FIFO_FIFO_MODE               0x40
#define LIS3DH_FIFO_STREAM_MODE             0x80
#define LIS3DH_FIFO_STREAM_TO_FIFO_MODE     0xC0

// Click Detection values
#define LIS3DH_SINGLE_CLICK                 0x15
#define LIS3DH_DOUBLE_CLICK                 0x2A

#define LIS3DH_MODE_NORMAL                  0x00
#define LIS3DH_MODE_LOW_POWER               0x01
#define LIS3DH_MODE_HIGH_RESOLUTION         0x02

#define LIS3DH_ADC1                         0x01
#define LIS3DH_ADC2                         0x02
#define LIS3DH_ADC3                         0x03

// Record for accelerometer readings
typedef struct {
    double  x;
    double  y;
    double  z;
} AccelResult;

// Record for interrupt table readings
typedef struct {
    bool    int_1;
    bool    x_low;
    bool    x_high;
    bool    y_low;
    bool    y_high;
    bool    z_low;
    bool    z_high;
    bool    click;
    bool    single_click;
    bool    double_click;
} InterruptTable;


/*
 *  PROTOTYPES
 */
bool        LIS3DH_init();
void        LIS3DH_enable(bool state);
void        LIS3DH_enable_ADC(bool state);
double      LIS3DH_read_ADC(uint8_t adc_line);
void        LIS3DH_get_accel(AccelResult* result);
uint8_t     LIS3DH_get_range();
uint8_t     LIS3DH_set_range(uint8_t rangeA);
uint32_t    LIS3DH_set_data_rate(uint32_t rate);
void        LIS3DH_set_mode(uint8_t mode);
void        LIS3DH_configure_high_pass_filter(uint8_t filters, uint8_t cutoff, uint8_t mode);
void        LIS3DH_configure_Fifo(bool state, uint8_t fifomode);
void        LIS3DH_configure_click_irq(bool enable, uint8_t click_type, float threshold, uint8_t time_limit, uint8_t latency, uint8_t window);
void        LIS3DH_configure_irq_latching(bool enable);
void        LIS3DH_get_interrupt_table(InterruptTable* data);
void        LIS3DH_reset();
uint8_t     LIS3DH_get_device_id();

void        _set_reg(uint8_t reg, uint8_t val);
void        _set_reg_bit(uint8_t reg, uint8_t bit, bool state);
uint8_t     _get_reg(uint8_t reg);
void        _get_multi_reg(uint8_t reg, uint8_t* result, uint8_t num_bytes);

#endif      // _LIS3DH_HEADER_
