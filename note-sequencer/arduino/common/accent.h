#pragma once

#include <stdint.h>

typedef enum {
  ACCENT_TYPE_RISE,
  ACCENT_TYPE_FLAT,
  ACCENT_TYPE_FALL,
} accent_type_t;

uint16_t accent_value_at_time_index(
  accent_type_t accent_type, uint32_t accent_index, uint8_t accent_size);
