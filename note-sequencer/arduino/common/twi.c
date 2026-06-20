#include <stdbool.h>
#include <stdint.h>
#include "util.h"
#include "twi.h"

int twi_transmit_start(void) {
  // Attempt to transmit START.
  TWCR = BIT(TWINT)|BIT(TWSTA)|BIT(TWEN);

  // Wait for TWINT indicating START has been transmitted.
  while (!(TWCR & BIT(TWINT)));

  // Check that START was transmitted.
  if ((TWSR & 0xF8) != 0x08) {
    return -1;
  }

  return 0;
}

int twi_transmit_address(uint8_t address, bool write) {
  uint8_t mode_flag = write ? 0 : 1;

  // Transmit SLA+W.
  TWDR = (address << 1) | mode_flag;
  TWCR = BIT(TWINT) | BIT(TWEN);

  // Wait until SLA+W was ack'd.
  while (!(TWCR & BIT(TWINT)));

  // Check that SLA+W was ack'd.
  if ((TWSR & 0xF8) != 0x18) {
    return -1;
  }

  return 0;
}

void twi_transmit_data_start(uint8_t data) {
  TWDR = data;
}

int twi_transmit_data_end(void) {
  TWCR = BIT(TWINT) | BIT(TWEN);

  // Wait until data was ack'd.
  while (!(TWCR & BIT(TWINT)));

  // Check that the data was ack'd.
  if ((TWSR & 0xF8)!= 0x28) {
    return -1;
  }

  return 0;
}

int twi_transmit_data(uint8_t data) {
  twi_transmit_data_start(data);
  return twi_transmit_data_end();
}

void twi_transmit_stop(void) {
  // Transmit STOP.
  TWCR = BIT(TWINT)|BIT(TWEN)|BIT(TWSTO);
}

int twi_send_bytes(uint8_t address, uint8_t *bytes, int nbytes) {
  int error;
  error = twi_transmit_start();
  if (error != 0) {
    return error;
  }
  error = twi_transmit_address(address, true);
  if (error != 0) {
    return error;
  }
  for (int i = 0; i < nbytes; i++) {
    error = twi_transmit_data(*bytes);
    if (error != 0) {
      return error;
    }
    bytes++;
  }
  twi_transmit_stop();
  return 0;
}

int twi_send_byte(uint8_t address, uint8_t byte) {
  return twi_send_bytes(address, &byte, 1);
}

void twi_sr(uint8_t address) {
  TWCR = BIT(TWEA) | BIT(TWEN) | BIT(TWIE);
  TWAR = address << 1;
}

void twi_interrupt_ack(void) {
  TWCR |= BIT(TWINT);
}
