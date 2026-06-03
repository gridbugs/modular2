#pragma once

#include <stdint.h>

#define NUM_NOTES 120

typedef struct {
  const char name[4];
  uint16_t dac_value;
} note_t;

note_t note_from_index(uint8_t index);
