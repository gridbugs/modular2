#pragma once

#include <stdint.h>

typedef struct {
  uint8_t rows[8];
} bitmap_t;

bitmap_t* font_get_ascii_bitmap(char ch);
