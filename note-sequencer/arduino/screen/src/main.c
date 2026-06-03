#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#include "uart_bitbanged.h"
#include "timer.h"
#include "twi.h"
#include "display.h"
#include "note.h"
#include "notes.h"

#define DISPLAY_BACKLIGHT_BRIGHTNESS 0x10
#define TWI_ADDRESS 0x42

#define BUFSIZE 64
char buf[BUFSIZE];
int buf_i = 0;

ISR(TWI_vect) {
  switch (TWSR) {
    case TWI_SR_STATUS_SLAW:
      buf_i = 0;
      break;
    case TWI_SR_STATUS_DATA:
      buf[buf_i] = TWDR;
      buf_i++;
      break;
    case TWI_SR_STATUS_STOP:
      buf[buf_i] = '\0';
      display_text(buf, 0, 0, WHITE, BLACK, 1);
      break;
    default:
      break;
  }

  twi_interrupt_ack();
}

int main(void) {
  timer2_init_pwm_port_d_bit_3(DISPLAY_BACKLIGHT_BRIGHTNESS);

  // Allow printing over UART (bitbanged for compatibility with fake arduinos
  // with faulty USB serial chips).
  USART0_bitbanged_init();

  display_init();

  display_clear(MAGENTA);
  display_text("purple", 20, 10, WHITE, BLACK, 1);
  display_text("earth", 30, 30, WHITE, BLACK, 1);
  display_text("hypoth-", 10, 50, WHITE, BLACK, 1);
  display_text("esis", 50, 70, WHITE, BLACK, 1);

  printf("Hello, World!\n\r");

  display_clear(BLACK);

  sei();

  twi_sr(TWI_ADDRESS);

  while(1);

  return 0;
}
