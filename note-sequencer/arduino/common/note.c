#include <stdint.h>
#include "note.h"

static const char* note_names[NOTES_PER_OCTAVE] = {
  "C ",
  "C#",
  "D ",
  "D#",
  "E ",
  "F ",
  "F#",
  "G ",
  "G#",
  "A ",
  "A#",
  "B ",
};

const char* note_name(uint8_t note_index) {
  return note_names[note_index % NOTES_PER_OCTAVE];
}
