#include <stdint.h>
#include <stdbool.h>
#include "twi.h"

#define ADDR0 0x60
#define ADDR1 0x61

#define COMMAND_UPDATE 64

static int set_value(uint8_t address, uint16_t value) {
  int error;
  error = twi_transmit_start();
  if (error != 0) {
    return error;
  }
  error = twi_transmit_address(address, true);
  if (error != 0) {
    return error;
  }
  error = twi_transmit_data(COMMAND_UPDATE);
  if (error != 0) {
    return error;
  }
  error = twi_transmit_data((uint8_t)(value >> 4));
  if (error != 0) {
    return error;
  }
  error = twi_transmit_data((uint8_t)((value & 0xf) << 4));
  if (error != 0) {
    return error;
  }
  twi_transmit_stop();
  return 0;
}

int dac0_set_value(uint16_t value) {
  return set_value(ADDR0, value);
}

int dac1_set_value(uint16_t value) {
  return set_value(ADDR1, value);
}
