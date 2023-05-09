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
static uint32_t bcd(uint32_t base);
static void     HT16K33_write_cmd(uint8_t cmd);


/*
 * GLOBALS
 */
extern               I2C_HandleTypeDef    i2c;

// The hex character set
static const char    CHARSET[19] = "\x3F\x06\x5B\x4F\x66\x6D\x7D\x07\x7F\x6F\x5F\x7C\x58\x5E\x7B\x71\x40\x63";

// Map display digits to bytes in the buffer
static const uint8_t POS[4] = {0, 2, 6, 8};
static uint8_t       display_buffer[17];


/*
 * HT16K33 Functions
 */

/**
 * @brief Power on the LEDs and set the brightness.
 */
void HT16K33_init(void) {
    
    HT16K33_write_cmd(0x21);     // System on
    HT16K33_write_cmd(0x81);     // Display on
    HT16K33_write_cmd(0xEF);     // Set brightness
    HT16K33_clear_buffer();
}


/**
 * @brief Issue a single command byte to the HT16K33.
 *
 * @param cmd: The single-byte command.
 */
static void HT16K33_write_cmd(uint8_t cmd) {
    
    HAL_I2C_Master_Transmit(&i2c, HT16K33_I2C_ADDR << 1, &cmd, 1, 100);
}


/**
 * @brief Clear the display buffer.
 *
 *  This does not clear the LED -- call `HT16K33_draw()`.
 */
void HT16K33_clear_buffer(void) {
    
    memset(display_buffer, 0x00, 17);
}


/**
 * @brief Write the display buffer out to the LED.
 */
void HT16K33_draw(void) {
    
    // Set up the buffer holding the data to be
    // transmitted to the LED
    uint8_t tx_buffer[17] = { 0 };
    memcpy(tx_buffer + 1, display_buffer, 16);

    // Display the buffer and flash the LED
    HAL_I2C_Master_Transmit(&i2c, HT16K33_I2C_ADDR << 1, tx_buffer, sizeof(tx_buffer), 100);
}


/**
 * @brief Write a decimal value to the display buffer.
 *
 *  @param value:   The value to write.
 *  @param decimal: `true` if digit 1's decimal point should be lit,
 *                  `false` otherwise.
 */
void HT16K33_show_value(int16_t value, bool decimal) {
    
    // Convert the value to BCD...
    uint16_t bcd_val = bcd(value);
    HT16K33_clear_buffer();
    HT16K33_set_number((bcd_val >> 12) & 0x0F, 0, false);
    HT16K33_set_number((bcd_val >> 8)  & 0x0F, 1, decimal);
    HT16K33_set_number((bcd_val >> 4)  & 0x0F, 2, false);
    HT16K33_set_number(bcd_val & 0x0F,         3, false);
}


/**
 *  @brief Write a single-digit decimal number to the display buffer at the specified digit.
 *
 *  @param  number:  The value to write.
 *  @param  digit:   The digit that will show the number.
 *  @param  has_dot: `true` if the digit's decimal point should be lit,
 *                   `false` otherwise.
 */
void HT16K33_set_number(uint8_t number, uint8_t digit, bool has_dot) {
    
    if (digit > 3) return;
    if (number > 9) return;
    HT16K33_set_alpha('0' + number, digit, has_dot);
}


/**
 * @brief Write a single character glyph to the display buffer at the specified digit.
 *
 *  Glyph values are 8-bit integers representing a pattern of set LED segments.
 *  The value is calculated by setting the bit(s) representing the segment(s) you want illuminated.
 *  Bit-to-segment mapping runs clockwise from the top around the outside of the matrix; the inner segment is bit 6:
 *       0
 *       _
 *   5 |   | 1
 *     |   |
 *       - <----- 6
 *   4 |   | 2
 *     | _ |
 *       3
 *
 *  @param glyph:   The glyph to write.
 *  @param digit:   The digit that will show the number.
 *  @param has_dot: `true` if the digit's decimal point should be lit,
 *                  `false` otherwise.
 */
void HT16K33_set_glyph(uint8_t glyph, uint8_t digit, bool has_dot) {
    
    if (digit > 3) return;
    display_buffer[POS[digit]] = glyph;
    if (has_dot) display_buffer[POS[digit]] |= 0x80;
}


/**
 * @brief Write a single character to the display buffer at the specified digit.
 *
 * @param chr:     The character to write.
 * @param digit:   The digit that will show the number.
 * @param has_dot: `true` if the digit's decimal point should be lit,
 *                 `false` otherwise.
 */
void HT16K33_set_alpha(char chr, uint8_t digit, bool has_dot) {
    
    if (digit > 3) return;

    uint8_t char_val = 0xFF;
    if (chr >= 'a' && chr <= 'f') {
        char_val = (uint8_t)chr - 87;
    } else if (chr >= '0' && chr <= '9') {
        char_val = (uint8_t)chr - 48;
    }

    if (char_val == 0xFF) return;
    display_buffer[POS[digit]] = CHARSET[char_val];
    if (has_dot) display_buffer[POS[digit]] |= 0x80;
}


/**
 * @brief Set or unset a digit's decimal point.
 *
 * @param digit:   The digit that will show the number.
 * @param is_set:  `true` if the digit's decimal point should be lit,
 *                 `false` otherwise.
 */
void HT16K33_set_point(uint8_t digit, bool is_set) {
    
    if (digit > 3) return;
    if (is_set) {
        display_buffer[POS[digit]] |= 0x80;
    } else {
        display_buffer[POS[digit]] &= 0x7F;
    }
}

/**
 * @brief Convert a 16-bit value (0-9999) to BCD notation.
 *
 * @param base: The value to convert.
 *
 * @returns The BCD form of the value.
 */
static uint32_t bcd(uint32_t base) {
    
    if (base > 9999) base = 9999;
    for (uint32_t i = 0 ; i < 16 ; ++i) {
        base = base << 1;
        if (i == 15) break;
        if ((base & 0x000F0000) > 0x0004FFFF) base += 0x00030000;
        if ((base & 0x00F00000) > 0x004FFFFF) base += 0x00300000;
        if ((base & 0x0F000000) > 0x04FFFFFF) base += 0x03000000;
        if ((base & 0xF0000000) > 0x4FFFFFFF) base += 0x30000000;
    }

    return (base >> 16) & 0xFFFF;
}
