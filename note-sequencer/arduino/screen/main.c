#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#include "uart.h"
#include "timer.h"
#include "twi.h"
#include "display.h"
#include "note.h"
#include "note_indices.h"

#define DISPLAY_BACKLIGHT_BRIGHTNESS 0x10
#define TWI_ADDRESS 0x42

#define COMMAND_BUF_SIZE 64
uint8_t command_buf[COMMAND_BUF_SIZE];
int command_buf_i = 0;

#define STRING_BUF_SIZE 64
char string_buf[STRING_BUF_SIZE];

ISR(TWI_vect) {
  switch (TWSR) {
    case TWI_SR_STATUS_SLAW:
      command_buf_i = 0;
      break;
    case TWI_SR_STATUS_DATA:
      command_buf[command_buf_i] = TWDR;
      command_buf_i++;
      break;
    case TWI_SR_STATUS_STOP:
      uint8_t note_index = command_buf[0];
      strncpy(string_buf, note_name(note_index), 2);
      string_buf[2] = '0' + note_octave(note_index);
      string_buf[3] = '\0';
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
  USART0_init();

  display_init();

  display_clear(MAGENTA);
  display_text("purple", 20, 10, WHITE, BLACK, 1);
  display_text("earth", 30, 30, WHITE, BLACK, 1);
  display_text("hypoth-", 10, 50, WHITE, BLACK, 1);
  display_text("esis", 50, 70, WHITE, BLACK, 1);

  display_clear(BLACK);

  sei();

  twi_sr(TWI_ADDRESS);

  while(1) {
    display_text(string_buf, 0, 0, WHITE, BLACK, 1);
  }

  return 0;
}
