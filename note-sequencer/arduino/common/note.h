#pragma once

#include <stdint.h>

#define NUM_NOTES 120
#define NOTES_PER_OCTAVE 12

static inline uint8_t note_octave(uint8_t note_index) {
  return note_index / NOTES_PER_OCTAVE;
}

const char* note_name(uint8_t note_index);

// Note-per-octave on a 12-bit DAC
static inline uint16_t note_dac_value(uint8_t note_index) {
  return note_index << 5;
}
