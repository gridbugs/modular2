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
#include "state.h"

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

#define SEQUENCE_TOP_Y 32

void state_render_cursor(state_t *state, char cursor_char, int fg, int bg) {
  int cursor_x = ((state->current_index * 2) / MAX_NUM_STEPS) * 64;
  int cursor_y = SEQUENCE_TOP_Y + ((state->current_index % (MAX_NUM_STEPS / 2)) * 8);
  char buf[] = { cursor_char, '\0' };
  display_text(buf, cursor_x, cursor_y, fg, bg, 0);
}

void state_render_step(state_t *state, int step_index, int fg, int bg) {
  static char buf[8];
  int x = ((step_index * 2) / MAX_NUM_STEPS) * 64;
  int y = SEQUENCE_TOP_Y + ((step_index % (MAX_NUM_STEPS / 2)) * 8);
  int index_fg = GREY;
  sprintf(buf, "%02d", step_index + 1);
  display_text(buf, x + 8, y, index_fg, bg, 0);
  step_t *step = &state->sequence.steps[step_index];
  if (step->enabled) {
    const char* name = note_name(step->note_index);
    uint8_t octave = note_octave(step->note_index);
    char accent = step->accent ? 'a' : ' ';
    char glide = step->glide ? 'g' : ' ';
    sprintf(buf, "%s%d%c%c", name, octave, accent, glide);
    display_text(buf, x + 24, y, fg, bg, 0);
  } else {
    display_text("  -    ", x + 24, y, fg, bg, 0);
  }
}

void state_render(state_t *state) {
  int fg = WHITE;
  int bg = BLACK;
  for (int i = 0; i < MAX_NUM_STEPS; i++) {
    state_render_step(state, i, fg, bg);
  }
  state_render_cursor(state, '>', fg, bg);
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

  state_t state = state_new();
  state.sequence.steps[1] = (step_t) {
    .note_index = 42,
    .enabled = true,
    .accent = true,
    .glide = true,
  };
  state_render(&state);

  sei();

  twi_sr(TWI_ADDRESS);

  while(1) {
    display_text(string_buf, 0, 0, WHITE, BLACK, 1);
  }

  return 0;
}
