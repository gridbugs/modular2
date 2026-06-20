#pragma once

#include <stdio.h>
#include <stdint.h>

#define SCREEN_ARDUINO_TWI_ADDRESS 0x42

typedef enum {
  COMMAND_HELLO,
  COMMAND_SHOW_SPLASH,
  COMMAND_SHOW_UI,
  COMMAND_SET_NOTE,
} command_type_t;

typedef struct {
  command_type_t typ;
  union {
    struct {
      uint8_t note_index;
    } set_note;
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

command_t command_from_bytes(uint8_t *bytes);
int command_send(command_t command);
