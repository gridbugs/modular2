#pragma once

#include "types.h"
#include <stdint.h>

uint16_t accent_value_at_time_index(
  accent_type_t accent_type,
  uint32_t accent_index,
  uint8_t accent_size);
