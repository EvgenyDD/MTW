#include "sh1107.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdarg.h>
#include <string.h>

#define NUM_PAGES 16

static char buf_print[256];
static uint8_t disp_buf[8 * 128] = {0};
static bool update_pending = true;
static int upd_phase = -1;

#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22

static char buf_print[256];
static uint8_t disp_buf[LCD_W * (LCD_H / 8)];

static void cmd_write(uint8_t cmd)
{
	i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
	i2c_master_start(cmd_handle);
	i2c_master_write_byte(cmd_handle, (OLED_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd_handle, 0x00, true);
	i2c_master_write_byte(cmd_handle, cmd, true);
	i2c_master_stop(cmd_handle);
	i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd_handle);
}

static void data_write(uint8_t *data, size_t len)
{
	i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
	i2c_master_start(cmd_handle);
	i2c_master_write_byte(cmd_handle, (OLED_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd_handle, 0x40, true);
	i2c_master_write(cmd_handle, data, len, true);
	i2c_master_stop(cmd_handle);
	i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd_handle);
}

static void update_continue(void)
{
	if(upd_phase >= NUM_PAGES || upd_phase == -1)
	{
		upd_phase = -1;
		return;
	}
	cmd_write(0x10);			 // set column address
	cmd_write(0x00);			 // set column address
	cmd_write(0xB0 + upd_phase); // set page address

	data_write(&disp_buf[upd_phase * 64], 64);
	upd_phase++;
}

static void update_start(void)
{
	if(upd_phase != -1) return;

	update_pending = false;
	upd_phase = 0;
	update_continue();
}

void sh1107_init(void)
{
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = 32,
		.scl_io_num = 33,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};

	i2c_param_config(I2C_MASTER_NUM, &conf);
	i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

	vTaskDelay(100 / portTICK_PERIOD_MS);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, 0, true);
	i2c_master_write_byte(cmd, 0xAE, true);			// Turn display off
	i2c_master_write_byte(cmd, 0xDC, true);			// Set display start line
	i2c_master_write_byte(cmd, 0x00, true);			// ...value
	i2c_master_write_byte(cmd, 0x81, true);			// Set display contrast
	i2c_master_write_byte(cmd, 0x2F, true);			// ...value
	i2c_master_write_byte(cmd, 0x20, true);			// Set memory mode
	i2c_master_write_byte(cmd, 0xA0 + 1 * 1, true); // Non-rotated display
	i2c_master_write_byte(cmd, 0xC0 + 8 * 1, true); // Non-flipped vertical
	i2c_master_write_byte(cmd, 0xA8, true);			// Set multiplex ratio
	i2c_master_write_byte(cmd, 0x7F, true);			// ...value
	i2c_master_write_byte(cmd, 0xD3, true);			// Set display offset to zero
	i2c_master_write_byte(cmd, 0x60, true);			// ...value
	i2c_master_write_byte(cmd, 0xD5, true);			// Set display clock divider
	i2c_master_write_byte(cmd, 0x51, true);			// ...value
	i2c_master_write_byte(cmd, 0xD9, true);			// Set pre-charge
	i2c_master_write_byte(cmd, 0x22, true);			// ...value
	i2c_master_write_byte(cmd, 0xDB, true);			// Set com detect
	i2c_master_write_byte(cmd, 0x35, true);			// ...value
	i2c_master_write_byte(cmd, 0xB0, true);			// Set page address
	i2c_master_write_byte(cmd, 0xDA, true);			// Set com pins
	i2c_master_write_byte(cmd, 0x12, true);			// ...value
	i2c_master_write_byte(cmd, 0xA4, true);			// output ram to display
	i2c_master_write_byte(cmd, 0xA6, true);			// Non-inverted display
	i2c_master_write_byte(cmd, 0xad, true);			// external VPP
	i2c_master_write_byte(cmd, 0x80, true);			// ...value
	i2c_master_write_byte(cmd, 0xAF, true);			// Turn display on
	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
	update_pending = true;
}

bool sh1107_is_update_pending(void) { return update_pending && upd_phase == -1; }
void sh1107_set_update_pending(void) { update_pending = true; }

void sh1107_poll(void)
{
	if(sh1107_is_update_pending())
	{
		update_start();
		while(upd_phase != -1)
			update_continue();
	}
}

void sh1107_clear(void)
{
	memset(disp_buf, 0, sizeof(disp_buf));
}

void sh1107_set_power(int on)
{
	cmd_write(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void sh1107_pixel(int x, int y, bool filled)
{
	if(x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return;
	x = LCD_W - 1 - x;

	int page = y / 8;
	int bit = y % 8;
	if(filled)
		disp_buf[x + page * LCD_W] |= (1 << bit);
	else
		disp_buf[x + page * LCD_W] &= (uint8_t)~(1 << bit);
}

void sh1107_line(int x0, int y0, int x1, int y1)
{
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx - dy;

	while(1)
	{
		sh1107_pixel(x0, y0, true);
		if(x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if(e2 > -dy)
		{
			err -= dy;
			x0 += sx;
		}
		if(e2 < dx)
		{
			err += dx;
			y0 += sy;
		}
	}
}

void sh1107_rect(int x, int y, int w, int h)
{
	sh1107_line(x, y, x + w, y);
	sh1107_line(x + w, y, x + w, y + h);
	sh1107_line(x + w, y + h, x, y + h);
	sh1107_line(x, y + h, x, y);
}

void sh1107_fill_rect(int x, int y, int w, int h)
{
	for(int i = x; i < x + w; i++)
	{
		for(int j = y; j < y + h; j++)
		{
			sh1107_pixel(i, j, true);
		}
	}
}

void sh1107_char(uint16_t x, uint16_t y, const char c, const uint8_t *font, uint32_t font_sz, bool invert)
{
	uint32_t num_ch = 0;
	if(font[3] == 0 || font[3] == 1) num_ch = font_sz / font[0] / (font[1] / 8 + (font[1] % 8 ? 1 : 0));
	if(font[3] == 2 || font[3] == 3) num_ch = font_sz / font[1] / (font[0] / 8 + (font[0] % 8 ? 1 : 0));

	if(font[3] == 2)
	{
		if(x + font[0] >= LCD_W || y + font[1] >= LCD_H) return;
		if(c < font[2]) return;
		uint32_t ch_offset = 4 + (c - font[2]) * font[1] * (font[0] / 8 + (font[0] % 8 ? 1 : 0));

		for(uint16_t row = 0; row < font[1]; row++)
		{
			for(uint16_t col = 0; col < font[0]; col++)
			{
				sh1107_pixel(x + col, y + (font[1] - 1) - row,
							 !!(font[ch_offset] & (0x80 >> (col % 8))) ^ invert);
				if(col % 8 == 7) ch_offset++;
			}
			if(font[0] % 8) ch_offset++;
		}
	}
	else
	{
		if(x + font[0] >= LCD_W || y + font[1] >= LCD_H) return;

		if(c >= font[2] + num_ch) return;
		if(c < font[2]) return;
		uint32_t ch_offset = 4 + (uint32_t)((c - font[2]) * ((uint32_t)((font[0]) < 8
																			? font[0] * ((font[1] + 7) / 8)
																			: (font[0] * font[1] + 7) / 8)));

		if(font[3] == 3)
		{
			for(int16_t j = 0; j < font[1]; j++)
			{
				for(int8_t k = 0; k < font[0] / 8; k++)
				{
					const char frame = font[ch_offset++];
					for(int8_t i = 0; i < 8; i++)
					{
						sh1107_pixel(x + (8 - 1) - i, y + (font[1] - 1) - j,
									 !!(frame & (1 << i)) ^ invert);
					}
				}
			}
		}
		else
		{
			for(int8_t row = font[1] - 1; row >= 0; row--)
			{
				for(uint16_t col = 0; col < font[0]; col++)
				{
					sh1107_pixel(x + col, y + row,
								 !!(font[ch_offset + col] & (1 << (font[3] == 1 ? 7 - row : row))) ^ invert);
				}
			}
		}
	}
}

void sh1107_string(uint16_t x, uint16_t y, const uint8_t *font, uint32_t font_sz, bool invert, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(buf_print, sizeof(buf_print), format, args);
	va_end(args);
	for(char *it = buf_print; *it; it++)
	{
		sh1107_char(x, y, *it, font, font_sz, invert);
		x += font[0] + 1;
	}
}

void sh1107_dvd(void)
{
	static int32_t p[2] = {0, 0};
	static int32_t speed[2] = {1, 1};

#define DVD_FONT font5x8
	const int32_t size[2] = {5 * (DVD_FONT[0] + 1) - 1, 2 * DVD_FONT[1] + 1};

	sh1107_clear();
	// sh1107_rect(p[0], p[1], size[0], size[1]);
	sh1107_string(p[0] + DVD_FONT[0] + 1, p[1] + DVD_FONT[1] + 1, F(font5x8), false, "DVD");
	sh1107_string(p[0], p[1], F(font5x8), false, "VIDEO");
	p[0] += speed[0];
	p[1] += speed[1];
	if(p[0] == 0 && speed[0] < 0) speed[0] *= -1;
	if(p[1] == 0 && speed[1] < 0) speed[1] *= -1;
	if(p[0] + size[0] >= LCD_W - 1 && speed[0] > 0) speed[0] *= -1;
	if(p[1] + size[1] >= LCD_H - 1 && speed[1] > 0) speed[1] *= -1;
	update_pending = true;
}