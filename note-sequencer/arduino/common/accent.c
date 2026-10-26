#include "accent.h"
#include "debug.h"
#include "mcp4725.h"
#include <stdint.h>
#include <stdio.h>

static inline uint16_t scale_output(uint8_t output, uint8_t accent_size) {
  return (uint16_t)(((uint32_t)output * (uint32_t)DAC_MAX * (uint32_t)accent_size) /
                    ((uint32_t)255 * (uint32_t)255));
}

uint16_t accent_value_at_time_index(
  accent_type_t accent_type,
  uint32_t accent_index,
  uint8_t accent_size) {
  if (accent_index > 255) {
    return 0;
  }
  switch (accent_type) {
    case ACCENT_TYPE_FLAT: {
      return scale_output(255, accent_size);
    }
    case ACCENT_TYPE_RISE: {
      return scale_output(accent_index, accent_size);
    }
    case ACCENT_TYPE_FALL: {
      return scale_output(255 - accent_index, accent_size);
    }
  }
  PANIC("Unexpected accent type: %d", accent_type);
}
