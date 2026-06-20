#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include "util.h"
#include "twi.h"

int twi_transmit_start(void) {
  // Attempt to transmit START.
  TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);

  // Wait for TWINT indicating START has been transmitted.
  while (!(TWCR & (1<<TWINT)));

  // Check that START was transmitted.
  if ((TWSR & 0xF8) != 0x08) {
    printf("start error %x\n\r", TWSR);
    return -1;
  }

  return 0;
}

int twi_transmit_address(uint8_t address, bool write) {
  uint8_t mode_flag = write ? 0 : 1;

  // Transmit SLA+W.
  TWDR = (address << 1) | mode_flag;
  TWCR = (1<<TWINT) | (1<<TWEN);

  // Wait until SLA+W was ack'd.
  while (!(TWCR & (1<<TWINT)));

  // Check that SLA+W was ack'd.
  if ((TWSR & 0xF8) != 0x18) {
    printf("tx addr err %x\n\r", TWSR);
    return -1;
  }

  return 0;
}

void twi_transmit_data_start(uint8_t data) {
  TWDR = data;
}

int twi_transmit_data_end(void) {
  TWCR = (1<<TWINT) | (1<<TWEN);

  // Wait until data was ack'd.
  while (!(TWCR & (1<<TWINT)));

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
  TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}

int twi_send_bytes(uint8_t address, uint8_t *bytes, int nbytes) {
  int error;
  error = twi_transmit_start();
  if (error != 0) {
    goto end;
  }
  error = twi_transmit_address(address, true);
  if (error != 0) {
    goto end;
  }
  for (int i = 0; i < nbytes; i++) {
    error = twi_transmit_data(*bytes);
    if (error != 0) {
      goto end;
    }
    bytes++;
  }

end:
  twi_transmit_stop();
  return error;
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
