#pragma once

#include <stdint.h>

// Some ready-made 16-bit ('565') color settings:
#define ST77XX_BLACK 0x0000
#define ST77XX_WHITE 0xFFFF
#define ST77XX_RED 0xF800
#define ST77XX_GREEN 0x07E0
#define ST77XX_BLUE 0x001F
#define ST77XX_CYAN 0x07FF
#define ST77XX_MAGENTA 0xF81F
#define ST77XX_YELLOW 0xFFE0
#define ST77XX_ORANGE 0xFC00
#define ST77XX_GREY 0x7BEF

#define ST7735_WIDTH 128
#define ST7735_HEIGHT 128

typedef struct {
  uint8_t x;
  uint8_t y;
  uint8_t w;
  uint8_t h;
} window_t;

void st7735_init(void);

// Begin streaming pixels into the specified window
void st7735_prepare_for_window(window_t window);

// Stream a single pixel after calling `st7735_prepare_for_window`.
// Call this as many times as necessary.
void st7735_send_colour(uint16_t colour);

// Finalize a stream of pixels.
void st7735_finalize(void);
