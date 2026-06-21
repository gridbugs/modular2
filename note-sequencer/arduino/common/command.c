#include "command.h"
#include "twi.h"

int command_type_num_bytes(command_type_t command_type) {
  switch (command_type) {
    case COMMAND_HELLO:
    case COMMAND_SHOW_SPLASH:
    case COMMAND_SHOW_UI:
      return 1;
    case COMMAND_SET_NOTE:
    case COMMAND_SET_SEQUENCE_INDEX:
    case COMMAND_CLEAR_SEQUENCE_NOTE:
      return 2;
    case COMMAND_SET_SEQUENCE_NOTE:
    case COMMAND_SET_STEP_FLAGS:
      return 3;
  }
  return 0;
}

int command_to_bytes(command_t command, uint8_t *bytes) {
  bytes[0] = command.typ;
  switch (command.typ) {
    case COMMAND_HELLO:
    case COMMAND_SHOW_SPLASH:
    case COMMAND_SHOW_UI:
      return 1;
    case COMMAND_SET_NOTE:
      bytes[1] = command.args.set_note.note_index;
      return 2;
    case COMMAND_SET_SEQUENCE_INDEX:
      bytes[1] = command.args.set_sequence_index.sequence_index;
      return 2;
    case COMMAND_SET_SEQUENCE_NOTE:
      bytes[1] = command.args.set_sequence_note.sequence_index;
      bytes[2] = command.args.set_sequence_note.note_index;
      return 3;
    case COMMAND_CLEAR_SEQUENCE_NOTE:
      bytes[1] = command.args.clear_sequence_note.sequence_index;
      return 2;
    case COMMAND_SET_STEP_FLAGS:
      bytes[1] = command.args.set_step_flags.sequence_index;
      bytes[2] = command.args.set_step_flags.flags;
      return 3;
  }
  return 0;
}

int commands_to_bytes(command_t *commands, uint8_t num_commands, uint8_t *bytes) {
  bytes[0] = num_commands;
  int n = 1;
  for (int i = 0; i < num_commands; i++) {
    n += command_to_bytes(commands[i], bytes + n);
  }
  return n;
}

command_t command_from_bytes(uint8_t *bytes) {
  switch (bytes[0]) {
    case COMMAND_HELLO:
      return command_hello();
    case COMMAND_SHOW_SPLASH:
      return command_show_splash();
    case COMMAND_SHOW_UI:
      return command_show_ui();
    case COMMAND_SET_NOTE:
      return command_set_note(bytes[1]);
    case COMMAND_SET_SEQUENCE_INDEX:
      return command_set_sequence_index(bytes[1]);
    case COMMAND_SET_SEQUENCE_NOTE:
      return command_set_sequence_note(bytes[1], bytes[2]);
    case COMMAND_CLEAR_SEQUENCE_NOTE:
      return command_clear_sequence_note(bytes[1]);
    case COMMAND_SET_STEP_FLAGS:
      return command_set_step_flags(bytes[1], bytes[2]);
    default:
      printf("Unexpected command type: %d\n\r", bytes[0]);
      while(1);
  }
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
