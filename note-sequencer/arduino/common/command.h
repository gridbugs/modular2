#pragma once

#include "state.h"
#include <stdint.h>
#include <stdio.h>

#define SCREEN_ARDUINO_TWI_ADDRESS 0x42

typedef enum {
  // Print "Hello, World!\n\r" over the serial port (for debugging).
  COMMAND_HELLO,

  // Display the splash screen.
  COMMAND_SHOW_SPLASH,

  // Render the entire UI.
  COMMAND_SHOW_UI,

  // Set the currently active note.
  COMMAND_SET_NOTE,

  // Move the playback cursor to a given position.
  COMMAND_SET_PLAYBACK_INDEX,

  // Move the edit cursor to a given position.
  COMMAND_SET_EDIT_INDEX,

  // Change the note at a given index.
  COMMAND_SET_STEP_NOTE,

  // Clear the note at a given index.
  COMMAND_CLEAR_STEP_NOTE,

  // Set the flags at a given index.
  COMMAND_SET_STEP_FLAGS,

  // Set the live flags.
  COMMAND_SET_LIVE_FLAGS,

  // Clear all steps and flags.
  COMMAND_CLEAR_SEQUENCE,

  // Set the mode.
  COMMAND_SET_MODE,

  // Set or clear the live flag.
  COMMAND_SET_LIVE,

  // Switch into or out of the mode for setting a field.
  COMMAND_SETTING_FIELD,

  // Set the tempo.
  COMMAND_SET_TEMPO,

  // Set the gate.
  COMMAND_SET_GATE,

  // Set the glide.
  COMMAND_SET_GLIDE,

  // Indicate a change in the clock signal.
  COMMAND_SET_CLOCK,

  // Set the clock source.
  COMMAND_SET_CLOCK_SOURCE,

  // Set the octave.
  COMMAND_SET_OCTAVE,

  // Set the edit mode.
  COMMAND_SET_EDIT_MODE,

  // Set the accent type.
  COMMAND_SET_ACCENT_TYPE,

  // Set the duration of the accent.
  COMMAND_SET_ACCENT_DURATION_RATIO,

  // Set the size of the accent.
  COMMAND_SET_ACCENT_SIZE,
} command_type_t;

typedef struct {
  command_type_t typ;
  union {
    struct {
      uint8_t note_index;
    } set_note;
    struct {
      uint8_t to_note_index;
      uint8_t progress;
    } glide_note;
    struct {
      uint8_t sequence_index;
    } set_playback_index;
    struct {
      uint8_t sequence_index;
    } set_edit_index;
    struct {
      uint8_t sequence_index;
      uint8_t note_index;
    } set_step_note;
    struct {
      uint8_t sequence_index;
    } clear_step_note;
    struct {
      uint8_t sequence_index;
      uint8_t flags;
    } set_step_flags;
    struct {
      uint8_t flags;
    } set_live_flags;
    struct {
      mode_t mode;
    } set_mode;
    struct {
      bool live;
    } set_live;
    struct {
      setting_field_t setting_field;
    } setting_field;
    struct {
      uint8_t tempo;
    } set_tempo;
    struct {
      uint8_t gate;
    } set_gate;
    struct {
      uint8_t glide;
    } set_glide;
    struct {
      bool clock_state;
    } set_clock;
    struct {
      clock_source_t clock_source;
    } set_clock_source;
    struct {
      uint8_t octave;
    } set_octave;
    struct {
      edit_mode_t edit_mode;
    } set_edit_mode;
    struct {
      accent_type_t accent_type;
    } set_accent_type;
    struct {
      uint8_t accent_duration_ratio;
    } set_accent_duration_ratio;
    struct {
      uint8_t accent_size;
    } set_accent_size;
  } args;
} command_t;

static inline command_t command_hello(void) {
  return (command_t){.typ = COMMAND_HELLO};
}

static inline command_t command_show_splash(void) {
  return (command_t){.typ = COMMAND_SHOW_SPLASH};
}

static inline command_t command_show_ui(void) {
  return (command_t){.typ = COMMAND_SHOW_UI};
}

static inline command_t command_set_note(uint8_t note_index) {
  return (command_t){.typ = COMMAND_SET_NOTE,
    .args = {
      .set_note =
        {
          .note_index = note_index,
        },
    }};
}

static inline command_t command_set_playback_index(uint8_t sequence_index) {
  return (command_t){.typ = COMMAND_SET_PLAYBACK_INDEX,
    .args = {
      .set_playback_index =
        {
          .sequence_index = sequence_index,
        },
    }};
}

static inline command_t command_set_edit_index(uint8_t sequence_index) {
  return (command_t){.typ = COMMAND_SET_EDIT_INDEX,
    .args = {
      .set_edit_index =
        {
          .sequence_index = sequence_index,
        },
    }};
}

static inline command_t command_set_step_note(uint8_t sequence_index, uint8_t note_index) {
  return (command_t){.typ = COMMAND_SET_STEP_NOTE,
    .args = {
      .set_step_note =
        {
          .sequence_index = sequence_index,
          .note_index = note_index,
        },
    }};
}

static inline command_t command_clear_step_note(uint8_t sequence_index) {
  return (command_t){.typ = COMMAND_CLEAR_STEP_NOTE,
    .args = {
      .clear_step_note =
        {
          .sequence_index = sequence_index,
        },
    }};
}

static inline command_t command_set_step_flags(uint8_t sequence_index, uint8_t flags) {
  return (command_t){.typ = COMMAND_SET_STEP_FLAGS,
    .args = {
      .set_step_flags =
        {
          .sequence_index = sequence_index,
          .flags = flags,
        },
    }};
}

static inline command_t command_set_live_flags(uint8_t flags) {
  return (command_t){.typ = COMMAND_SET_LIVE_FLAGS,
    .args = {
      .set_live_flags =
        {
          .flags = flags,
        },
    }};
}

static inline command_t command_clear_sequence(void) {
  return (command_t){.typ = COMMAND_CLEAR_SEQUENCE};
}

static inline command_t command_set_mode(mode_t mode) {
  return (command_t){.typ = COMMAND_SET_MODE,
    .args = {
      .set_mode =
        {
          .mode = mode,
        },
    }};
}

static inline command_t command_set_live(bool live) {
  return (command_t){.typ = COMMAND_SET_LIVE,
    .args = {
      .set_live =
        {
          .live = live,
        },
    }};
}

static inline command_t command_setting_field(setting_field_t setting_field) {
  return (command_t){.typ = COMMAND_SETTING_FIELD,
    .args = {
      .setting_field =
        {
          .setting_field = setting_field,
        },
    }};
}

static inline command_t command_set_tempo(uint8_t tempo) {
  return (command_t){.typ = COMMAND_SET_TEMPO,
    .args = {
      .set_tempo =
        {
          .tempo = tempo,
        },
    }};
}

static inline command_t command_set_gate(uint8_t gate) {
  return (command_t){.typ = COMMAND_SET_GATE,
    .args = {
      .set_gate =
        {
          .gate = gate,
        },
    }};
}

static inline command_t command_set_glide(uint8_t glide) {
  return (command_t){.typ = COMMAND_SET_GLIDE,
    .args = {
      .set_glide =
        {
          .glide = glide,
        },
    }};
}

static inline command_t command_set_clock(bool clock_state) {
  return (command_t){.typ = COMMAND_SET_CLOCK,
    .args = {
      .set_clock =
        {
          .clock_state = clock_state,
        },
    }};
}

static inline command_t command_set_clock_source(clock_source_t clock_source) {
  return (command_t){.typ = COMMAND_SET_CLOCK_SOURCE,
    .args = {
      .set_clock_source =
        {
          .clock_source = clock_source,
        },
    }};
}

static inline command_t command_set_octave(uint8_t octave) {
  return (command_t){.typ = COMMAND_SET_OCTAVE,
    .args = {
      .set_octave =
        {
          .octave = octave,
        },
    }};
}

static inline command_t command_set_edit_mode(edit_mode_t edit_mode) {
  return (command_t){.typ = COMMAND_SET_EDIT_MODE,
    .args = {
      .set_edit_mode =
        {
          .edit_mode = edit_mode,
        },
    }};
}

static inline command_t command_set_accent_type(accent_type_t accent_type) {
  return (command_t){.typ = COMMAND_SET_ACCENT_TYPE,
    .args = {
      .set_accent_type =
        {
          .accent_type = accent_type,
        },
    }};
}

static inline command_t command_set_accent_duration_ratio(uint8_t accent_duration_ratio) {

  return (command_t){.typ = COMMAND_SET_ACCENT_DURATION_RATIO,
    .args = {
      .set_accent_duration_ratio =
        {
          .accent_duration_ratio = accent_duration_ratio,
        },
    }};
}

static inline command_t command_set_accent_size(uint8_t accent_size) {
  return (command_t){.typ = COMMAND_SET_ACCENT_SIZE,
    .args = {
      .set_accent_size =
        {
          .accent_size = accent_size,
        },
    }};
}

uint8_t commands_from_bytes(uint8_t *bytes, command_t *commands);
int command_send(command_t command);
int commands_send(command_t *commands, uint8_t num_commands);
