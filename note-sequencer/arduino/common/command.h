#pragma once

#include <stdio.h>
#include <stdint.h>
#include "state.h"

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

  // Move the cursor to a given position.
  COMMAND_SET_SEQUENCE_INDEX,

  // Change the note at a given index.
  COMMAND_SET_STEP_NOTE,

  // Clear the note at a given index.
  COMMAND_CLEAR_STEP_NOTE,

  // Set the flags at a given index
  COMMAND_SET_STEP_FLAGS,

  // Set the mode
  COMMAND_SET_MODE,
} command_type_t;

typedef struct {
  command_type_t typ;
  union {
    struct {
      uint8_t note_index;
    } set_note;
    struct {
      uint8_t sequence_index;
    } set_sequence_index;
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
      mode_t mode;
    } set_mode;
  } args;
} command_t;

static inline command_t command_hello(void) {
  return (command_t) { .typ = COMMAND_HELLO };
}

static inline command_t command_show_splash(void) {
  return (command_t) { .typ = COMMAND_SHOW_SPLASH };
}

static inline command_t command_show_ui(void) {
  return (command_t) { .typ = COMMAND_SHOW_UI };
}

static inline command_t command_set_note(uint8_t note_index) {
  return (command_t) {
    .typ = COMMAND_SET_NOTE,
    .args = {
      .set_note = {
        .note_index = note_index,
      },
    }
  };
}

static inline command_t command_set_sequence_index(uint8_t sequence_index) {
  return (command_t) {
    .typ = COMMAND_SET_SEQUENCE_INDEX,
    .args = {
      .set_sequence_index = {
        .sequence_index = sequence_index,
      },
    }
  };
}

static inline command_t command_set_step_note(uint8_t sequence_index, uint8_t note_index) {
  return (command_t) {
    .typ = COMMAND_SET_STEP_NOTE,
    .args = {
      .set_step_note = {
        .sequence_index = sequence_index,
        .note_index = note_index,
      },
    }
  };
}

static inline command_t command_clear_step_note(uint8_t sequence_index) {
  return (command_t) {
    .typ = COMMAND_CLEAR_STEP_NOTE,
    .args = {
      .clear_step_note = {
        .sequence_index = sequence_index,
      },
    }
  };
}

static inline command_t command_set_step_flags(uint8_t sequence_index, uint8_t flags) {
  return (command_t) {
    .typ = COMMAND_SET_STEP_FLAGS,
    .args = {
      .set_step_flags = {
        .sequence_index = sequence_index,
        .flags = flags,
      },
    }
  };
}

static inline command_t command_set_mode(mode_t mode) {
  return (command_t) {
    .typ = COMMAND_SET_MODE,
    .args = {
      .set_mode = {
        .mode = mode,
      },
    }
  };
}

uint8_t commands_from_bytes(uint8_t *bytes, command_t *commands);
int command_send(command_t command);
int commands_send(command_t *commands, uint8_t num_commands);
