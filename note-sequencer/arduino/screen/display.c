#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "st7735.h"
#include "font.h"
#include "util.h"
#include "display.h"

void display_init(void) {
  st7735_init();
}

void display_fill_window(window_t window, uint16_t colour) {
  st7735_prepare_for_window(window);
  for (int i = 0; i < (window.w * window.h); i++) {
    st7735_send_colour(colour);
  }
  st7735_finalize();
}

void display_clear(uint16_t colour) {
  display_fill_window((window_t) { .x = 0, .y = 0, .w = DISPLAY_WIDTH, .h = DISPLAY_HEIGHT }, colour);
}

void display_text(const char* text, uint8_t x, uint8_t y, uint16_t fg, uint16_t bg, uint8_t scale_radix) {
  size_t len = strlen(text);
  window_t window = (window_t) {
    .x = x,
    .y = y,
    .w = (len << 3) << scale_radix,
    .h = (1 << 3) << scale_radix,
  };
  st7735_prepare_for_window(window);
  for (int i = 0; i < window.h; i++) {
    for (int j = 0; j < window.w; j++) {
      char ch = text[(j >> 3) >> scale_radix];
      bitmap_t *bitmap = font_get_ascii_bitmap(ch);
      uint8_t row = bitmap->rows[i >> scale_radix];
      uint8_t col = (j >> scale_radix) & 7;
      uint16_t colour = bg;
      if (row & BIT(col)) {
        colour = fg;
      }
      st7735_send_colour(colour);
    }
  }
  st7735_finalize();
}
