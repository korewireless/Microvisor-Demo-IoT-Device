/**
 *
 * Microvisor IoT Device Demo
 *
 * Copyright © 2023, KORE Wireless
 * Licence: MIT
 *
 */
#ifndef _MCP9808_HEADER_
#define _MCP9808_HEADER_


/*
 *  CONSTANTS
 */
// Sensor I2C address
#define MCP9808_ADDR                0x18

// Register addresses
#define MCP9808_REG_CONFIG          0x01
#define MCP9808_REG_UPPER_TEMP      0x02
#define MCP9808_REG_LOWER_TEMP      0x03
#define MCP9808_REG_CRIT_TEMP       0x04
#define MCP9808_REG_AMBIENT_TEMP    0x05
#define MCP9808_REG_MANUF_ID        0x06
#define MCP9808_REG_DEVICE_ID       0x07


#ifdef __cplusplus
extern "C" {
#endif


/*
 *  PROTOTYPES
 */
bool        MCP9808_init(void) ;
double      MCP9808_read_temp(void);


#ifdef __cplusplus
}
#endif


#endif      // _MCP9808_HEADER_
