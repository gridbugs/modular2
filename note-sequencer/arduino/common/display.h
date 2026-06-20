#pragma once

#include <stdint.h>
#include "st7735.h"

#define BLACK ST77XX_BLACK
#define WHITE ST77XX_WHITE
#define RED ST77XX_RED
#define GREEN ST77XX_GREEN
#define BLUE ST77XX_BLUE
#define CYAN ST77XX_CYAN
#define MAGENTA ST77XX_MAGENTA
#define YELLOW ST77XX_YELLOW
#define ORANGE ST77XX_ORANGE
#define GREY ST77XX_GREY

#define DISPLAY_WIDTH ST7735_WIDTH
#define DISPLAY_HEIGHT ST7735_HEIGHT

void display_init(void);
void display_fill_window(window_t window, uint16_t colour);
void display_clear(uint16_t colour);
void display_text(const char* text, uint8_t x, uint8_t y, uint16_t fg, uint16_t bg, uint8_t scale_radix);
