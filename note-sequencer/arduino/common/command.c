#include "command.h"
#include "twi.h"

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
  }
  return 0;
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
    default:
      printf("Unexpected command type: %d\n\r", bytes[0]);
      while(1);
  }
}

int command_send(command_t command) {
  static uint8_t buf[4];
  int n = command_to_bytes(command, buf);
  return twi_send_bytes(SCREEN_ARDUINO_TWI_ADDRESS, buf, n);
}
