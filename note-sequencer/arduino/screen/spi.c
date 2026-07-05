#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"

#define PORTB_CLOCK_PIN 5
#define PORTB_MOSI_PIN 3
#define PORTB_ARDUINO_CHIP_SELECT_PIN 2

void spi_init(void) {
  DDRB |= BIT(PORTB_CLOCK_PIN) |
    BIT(PORTB_MOSI_PIN) |
    BIT(PORTB_ARDUINO_CHIP_SELECT_PIN);
  PORTB |= BIT(PORTB_ARDUINO_CHIP_SELECT_PIN);
  SPCR = BIT(SPE) | BIT(MSTR);
}

void spi_send(uint8_t data) {
  SPDR = data;
  while (!(SPSR & BIT(SPIF)));

  // XXX This shouldn't be necessary, but here we wait a bit longer before
  // returning. I observe the SPIF bit in SPSR getting set before the
  // transmission of the byte completes, so it's necessary to explicitly wait
  // here so I can rely on the write having completed after this function
  // returns.
  for (int i = 0; i < 6; i++) {
    __asm__ __volatile__("nop");
  }
}
