#include <stdint.h>
#include <stdbool.h>
#include "twi.h"
#include "mcp4725.h"

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
  error = twi_transmit_data(MCP4725_COMMAND_UPDATE);
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
  return set_value(MCP4725_ADDR0, value);
}

int dac1_set_value(uint16_t value) {
  return set_value(MCP4725_ADDR1, value);
}
