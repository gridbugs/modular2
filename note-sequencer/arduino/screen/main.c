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
#include "command.h"

#define DISPLAY_BACKLIGHT_BRIGHTNESS 0x10
#define TWI_ADDRESS 0x42

#define COMMAND_RING_SIZE 16

volatile command_t command_ring[COMMAND_RING_SIZE];
volatile unsigned long int command_ring_next_write_index = 0;
volatile unsigned long int command_ring_prev_read_index = 0;

uint8_t command_bytes[4];
int command_bytes_index = 0;

ISR(TWI_vect) {
  switch (TWSR) {
    case TWI_SR_STATUS_SLAW:
      break;
    case TWI_SR_STATUS_DATA:
      command_bytes[command_bytes_index] = TWDR;
      command_bytes_index++;
      break;
    case TWI_SR_STATUS_STOP:
      command_ring[command_ring_next_write_index % COMMAND_RING_SIZE] = command_from_bytes(command_bytes);
      command_ring_next_write_index++;
      command_bytes_index = 0;
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

void render_splash(void) {
  display_clear(MAGENTA);
  display_text("purple", 20, 20, WHITE, BLACK, 1);
  display_text("earth", 30, 40, WHITE, BLACK, 1);
  display_text("hypoth-", 10, 60, WHITE, BLACK, 1);
  display_text("esis", 50, 80, WHITE, BLACK, 1);
}

void handle_command(command_t command, state_t *state) {
  switch (command.typ) {
    case COMMAND_HELLO:
      printf("Hello, World!\n\r");
      break;
    case COMMAND_SHOW_SPLASH:
      render_splash();
      break;
    case COMMAND_SHOW_UI:
      display_clear(BLACK);
      state_render(state);
      break;
    case COMMAND_SET_NOTE:
      uint8_t note_index = command.args.set_note.note_index;
      char buf[4];
      sprintf(buf, "%s%d", note_name(note_index), note_octave(note_index));
      display_text(buf, 0, 0, WHITE, BLACK, 1);
      break;
  }
}

int main(void) {
  timer2_init_pwm_port_d_bit_3(DISPLAY_BACKLIGHT_BRIGHTNESS);

  // Allow printing over UART (bitbanged for compatibility with fake arduinos
  // with faulty USB serial chips).
  USART0_init();

  display_init();

  state_t state = state_new();
  state.sequence.steps[1] = (step_t) {
    .note_index = 42,
    .enabled = true,
    .accent = true,
    .glide = true,
  };

  sei();

  twi_sr(TWI_ADDRESS);

  while(1) {
    while (command_ring_prev_read_index < command_ring_next_write_index) {
      handle_command(command_ring[command_ring_prev_read_index % COMMAND_RING_SIZE], &state);
      command_ring_prev_read_index++;
    }
  }

  return 0;
}
