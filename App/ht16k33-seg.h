/**
 *
 * Microvisor IoT Device Demo
 * Version 3.0.1
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */
#ifndef _HT16K33_HEADER_
#define _HT16K33_HEADER_


#ifdef __cplusplus
extern "C" {
#endif


/*
 * CONSTANTS
 */
#define     HT16K33_I2C_ADDR            0x70


/*
 * PROTOTYPES
 */
void        HT16K33_init(void);
void        HT16K33_draw(void);
void        HT16K33_clear_buffer(void);
void        HT16K33_show_value(int16_t value, bool has_decimal);
void        HT16K33_set_alpha(char chr, uint8_t digit, bool has_dot);
void        HT16K33_set_number(uint8_t number, uint8_t digit, bool has_dot);
void        HT16K33_set_glyph(uint8_t glyph, uint8_t digit, bool has_dot);
void        HT16K33_set_point(uint8_t digit, bool is_set);


#ifdef __cplusplus
}
#endif


#endif  // _HT16K33_HEADER_
