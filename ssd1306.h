#ifndef SSD1306_H
#define SSD1306_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define SSD1306_I2C_ADDR        0x3C
#define SSD1306_HEIGHT          64
#define SSD1306_WIDTH           128

// Comandos
#define SSD1306_SET_CONTRAST    0x81
#define SSD1306_SET_ENTIRE_ON   0xA4
#define SSD1306_SET_NORM_INV    0xA6
#define SSD1306_SET_DISP        0xAE
#define SSD1306_SET_MEM_ADDR    0x20
#define SSD1306_SET_COL_ADDR    0x21
#define SSD1306_SET_PAGE_ADDR   0x22
#define SSD1306_SET_DISP_START_LINE 0x40
#define SSD1306_SET_SEG_REMAP   0xA0
#define SSD1306_SET_MUX_RATIO   0xA8
#define SSD1306_SET_COM_OUT_DIR 0xC0
#define SSD1306_SET_DISP_OFFSET 0xD3
#define SSD1306_SET_COM_PIN_CFG 0xDA
#define SSD1306_SET_DISP_CLK_DIV 0xD5
#define SSD1306_SET_PRECHARGE   0xD9
#define SSD1306_SET_VCOM_DESEL  0xDB
#define SSD1306_SET_CHARGE_PUMP 0x8D

typedef struct {
	i2c_inst_t *i2c_port;
	uint8_t address;
	uint8_t width;
	uint8_t height;
	uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
} ssd1306_t;

void ssd1306_init(ssd1306_t *display, i2c_inst_t *i2c_port, uint8_t address);
void ssd1306_clear(ssd1306_t *display);
void ssd1306_show(ssd1306_t *display);
void ssd1306_draw_pixel(ssd1306_t *display, int16_t x, int16_t y, bool on);
void ssd1306_draw_char(ssd1306_t *display, int16_t x, int16_t y, char c);
void ssd1306_draw_string(ssd1306_t *display, int16_t x, int16_t y, const char *str);

#endif // SSD1306_H
