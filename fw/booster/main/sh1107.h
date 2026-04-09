#ifndef SSD1306_I2C_H__
#define SSD1306_I2C_H__

#include "fonts.h"
#include <stdbool.h>
#include <stdint.h>

#define I2C_MASTER_SCL_IO 6
#define I2C_MASTER_SDA_IO 5
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define OLED_ADDRESS 0x3C

#define LCD_W 64
#define LCD_H 128
#define X_OFFSET 0
#define Y_OFFSET 0

void sh1107_init(void);

bool sh1107_is_update_pending(void);
void sh1107_set_update_pending(void);
void sh1107_poll(void);

void sh1107_clear(void);
void sh1107_set_power(int on);

void sh1107_pixel(int x, int y, bool filled);
void sh1107_line(int x0, int y0, int x1, int y1);
void sh1107_rect(int x, int y, int w, int h);
void sh1107_fill_rect(int x, int y, int w, int h);

void sh1107_char(uint16_t x, uint16_t y, const char c, const uint8_t *font, uint32_t font_sz, bool invert);
void sh1107_string(uint16_t x, uint16_t y, const uint8_t *font, uint32_t font_sz, bool invert, const char *format, ...) __attribute__((format(printf, 6, 7)));

void sh1107_dvd(void);

#endif // SSD1306_I2C_H__