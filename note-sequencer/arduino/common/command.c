#include "command.h"
#include "debug.h"
#include "twi.h"

int command_type_num_bytes(command_type_t command_type) {
  switch (command_type) {
    case COMMAND_HELLO:
    case COMMAND_SHOW_SPLASH:
    case COMMAND_SHOW_UI:
    case COMMAND_CLEAR_SEQUENCE:
      return 1;
    case COMMAND_SET_NOTE:
    case COMMAND_SET_PLAYBACK_INDEX:
    case COMMAND_SET_EDIT_INDEX:
    case COMMAND_CLEAR_STEP_NOTE:
    case COMMAND_SET_MODE:
    case COMMAND_SET_LIVE:
    case COMMAND_SETTING_FIELD:
    case COMMAND_SET_CLOCK:
    case COMMAND_SET_CLOCK_SOURCE:
    case COMMAND_SET_OCTAVE:
    case COMMAND_SET_LIVE_FLAGS:
    case COMMAND_SET_EDIT_MODE:
    case COMMAND_SET_ACCENT_TYPE:
      return 2;
    case COMMAND_SET_FIELD:
    case COMMAND_SET_STEP_NOTE:
    case COMMAND_SET_STEP_FLAGS:
      return 3;
  }
  return 0;
}

// Write an encoding of the command to a buffer returning the number of bytes
// written.
int command_to_bytes(command_t command, uint8_t *bytes) {
  bytes[0] = command.typ;
  switch (command.typ) {
    case COMMAND_HELLO:
    case COMMAND_SHOW_SPLASH:
    case COMMAND_SHOW_UI:
    case COMMAND_CLEAR_SEQUENCE:
      return 1;
    case COMMAND_SET_NOTE:
      bytes[1] = command.args.set_note.note_index;
      return 2;
    case COMMAND_SET_PLAYBACK_INDEX:
      bytes[1] = command.args.set_playback_index.sequence_index;
      return 2;
    case COMMAND_SET_EDIT_INDEX:
      bytes[1] = command.args.set_edit_index.sequence_index;
      return 2;
    case COMMAND_SET_STEP_NOTE:
      bytes[1] = command.args.set_step_note.sequence_index;
      bytes[2] = command.args.set_step_note.note_index;
      return 3;
    case COMMAND_CLEAR_STEP_NOTE:
      bytes[1] = command.args.clear_step_note.sequence_index;
      return 2;
    case COMMAND_SET_STEP_FLAGS:
      bytes[1] = command.args.set_step_flags.sequence_index;
      bytes[2] = command.args.set_step_flags.flags;
      return 3;
    case COMMAND_SET_LIVE_FLAGS:
      bytes[1] = command.args.set_live_flags.flags;
      return 2;
    case COMMAND_SET_MODE:
      bytes[1] = command.args.set_mode.mode;
      return 2;
    case COMMAND_SET_LIVE:
      bytes[1] = command.args.set_live.live;
      return 2;
    case COMMAND_SETTING_FIELD:
      bytes[1] = command.args.setting_field.setting_field;
      return 2;
    case COMMAND_SET_FIELD:
      bytes[1] = command.args.set_field.setting_field;
      bytes[2] = command.args.set_field.value;
      return 3;
    case COMMAND_SET_CLOCK:
      bytes[1] = command.args.set_clock.clock_state;
      return 2;
    case COMMAND_SET_CLOCK_SOURCE:
      bytes[1] = command.args.set_clock_source.clock_source;
      return 2;
    case COMMAND_SET_OCTAVE:
      bytes[1] = command.args.set_octave.octave;
      return 2;
    case COMMAND_SET_EDIT_MODE:
      bytes[1] = command.args.set_edit_mode.edit_mode;
      return 2;
    case COMMAND_SET_ACCENT_TYPE:
      bytes[1] = command.args.set_accent_type.accent_type;
      return 2;
  }
  __builtin_unreachable();
}

// Concatenate encodings of a sequence of commands into a buffer returning the
// number of bytes written.
int commands_to_bytes(command_t *commands, uint8_t num_commands, uint8_t *bytes) {
  bytes[0] = num_commands;
  int n = 1;
  for (int i = 0; i < num_commands; i++) {
    n += command_to_bytes(commands[i], bytes + n);
  }
  return n;
}

command_t command_from_bytes(uint8_t *bytes) {
  command_type_t command_type = bytes[0];
  switch (command_type) {
    case COMMAND_HELLO:
      return command_hello();
    case COMMAND_SHOW_SPLASH:
      return command_show_splash();
    case COMMAND_SHOW_UI:
      return command_show_ui();
    case COMMAND_SET_NOTE:
      return command_set_note(bytes[1]);
    case COMMAND_SET_PLAYBACK_INDEX:
      return command_set_playback_index(bytes[1]);
    case COMMAND_SET_EDIT_INDEX:
      return command_set_edit_index(bytes[1]);
    case COMMAND_SET_STEP_NOTE:
      return command_set_step_note(bytes[1], bytes[2]);
    case COMMAND_CLEAR_STEP_NOTE:
      return command_clear_step_note(bytes[1]);
    case COMMAND_SET_STEP_FLAGS:
      return command_set_step_flags(bytes[1], bytes[2]);
    case COMMAND_SET_LIVE_FLAGS:
      return command_set_live_flags(bytes[1]);
    case COMMAND_CLEAR_SEQUENCE:
      return command_clear_sequence();
    case COMMAND_SET_MODE:
      return command_set_mode(bytes[1]);
    case COMMAND_SET_LIVE:
      return command_set_live(bytes[1]);
    case COMMAND_SETTING_FIELD:
      return command_setting_field(bytes[1]);
    case COMMAND_SET_FIELD:
      return command_set_field(bytes[1], bytes[2]);
    case COMMAND_SET_CLOCK:
      return command_set_clock(bytes[1]);
    case COMMAND_SET_CLOCK_SOURCE:
      return command_set_clock_source(bytes[1]);
    case COMMAND_SET_OCTAVE:
      return command_set_octave(bytes[1]);
    case COMMAND_SET_EDIT_MODE:
      return command_set_edit_mode(bytes[1]);
    case COMMAND_SET_ACCENT_TYPE:
      return command_set_accent_type(bytes[1]);
  }
  PANIC("Unexpected command type: %d", bytes[0]);
}

uint8_t commands_from_bytes(uint8_t *bytes, command_t *commands) {
  uint8_t num_commands = bytes[0];
  bytes++;
  for (int i = 0; i < num_commands; i++) {
    command_t command = command_from_bytes(bytes);
    commands[i] = command;
    bytes += command_type_num_bytes(command.typ);
  }
  return num_commands;
}

int commands_send(command_t *commands, uint8_t num_commands) {
  static uint8_t buf[128];
  int n = commands_to_bytes(commands, num_commands, buf);
  return twi_send_bytes(SCREEN_ARDUINO_TWI_ADDRESS, buf, n);
}

int command_send(command_t command) {
  return commands_send(&command, 1);
}
