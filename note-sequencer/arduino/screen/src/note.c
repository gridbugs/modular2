#include <stdint.h>
#include "note.h"

static const char* note_names = "C C#D D#E F F#G G#A A#B ";

note_t note_from_index(uint8_t index) {
  uint8_t octave = index / 12;
  uint8_t name_offset = (index % 12) * 2;
  uint16_t dac_value = index << 5;
  return (note_t) {
    .name = { note_names[name_offset], note_names[name_offset + 1], '0' + octave, '\0' },
    .dac_value = dac_value,
  };
}
