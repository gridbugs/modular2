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

#define COMMAND_RING_SIZE 64

volatile command_t command_ring[COMMAND_RING_SIZE];
volatile unsigned long int command_ring_next_write_index = 0;
volatile unsigned long int command_ring_prev_read_index = 0;

uint8_t command_bytes[64];
int command_bytes_index = 0;
command_t command_buf[32];

ISR(TWI_vect) {
  switch (TWSR) {
    case TWI_SR_STATUS_SLAW:
      break;
    case TWI_SR_STATUS_DATA:
      command_bytes[command_bytes_index] = TWDR;
      command_bytes_index++;
      break;
    case TWI_SR_STATUS_STOP:
      if (command_ring_next_write_index < (command_ring_prev_read_index + COMMAND_RING_SIZE)) {
        int num_commands = commands_from_bytes(command_bytes, command_buf);
        for (int i = 0; i < num_commands; i++) {
          command_ring[command_ring_next_write_index % COMMAND_RING_SIZE] = command_buf[i];
          command_ring_next_write_index++;
        }
      }
      command_bytes_index = 0;
      break;
    default:
      break;
  }

  twi_interrupt_ack();
}

#define SEQUENCE_TOP_Y 32
#define MODE_LEFT_X 104
char buf[128];

void state_render_mode(state_t *state, int fg, int bg) {
  char* text;
  switch (state->mode) {
    case MODE_RUN:
      text = "RUN";
      break;
    case MODE_PROGRAM:
      text = "PRG";
      break;
    default:
      return;
  }
  display_text(text, MODE_LEFT_X, 0, fg, bg, 0);
}

void render_cursor_at(int index, char cursor_char, int fg, int bg) {
  int cursor_x = ((index * 2) / MAX_NUM_STEPS) * 64;
  int cursor_y = SEQUENCE_TOP_Y + ((index % (MAX_NUM_STEPS / 2)) * 8);
  char buf[] = { cursor_char, '\0' };
  display_text(buf, cursor_x, cursor_y, fg, bg, 0);
}

void state_render_cursor(state_t *state, char cursor_char, int fg, int bg) {
  render_cursor_at(state->current_index, cursor_char, fg, bg);
}

void state_render_step(state_t *state, int step_index, int fg, int bg) {
  int x = ((step_index * 2) / MAX_NUM_STEPS) * 64;
  int y = SEQUENCE_TOP_Y + ((step_index % (MAX_NUM_STEPS / 2)) * 8);
  int index_fg = GREY;
  sprintf(buf, "%02d", step_index + 1);
  display_text(buf, x + 8, y, index_fg, bg, 0);
  step_t *step = &state->sequence.steps[step_index];
  if (step->enabled) {
    const char* name = note_name(step->note_index);
    uint8_t octave = note_octave(step->note_index);
    sprintf(buf, "%s%d", name, octave);
    display_text(buf, x + 24, y, fg, bg, 0);
  } else {
    display_text(" -  ", x + 24, y, fg, bg, 0);
  }
  char accent = step_has_accent(step) ? 'a' : ' ';
  char glide = step_has_glide(step) ? 'g' : ' ';
  int flag_fg = step->enabled ? fg : GREY;
  sprintf(buf, "%c%c", accent, glide);
  display_text(buf, x + 48, y, flag_fg, bg, 0);
}

void state_render_setting(state_t *state, int fg, int bg) {
  int y = 112;
  if (state->setting_tempo) {
    sprintf(buf, "TEMPO: %3u BPM", state->tempo_bpm);
  } else {
    sprintf(buf, "              ");
  }
  display_text(buf, 8, y, fg, bg, 0);
}

void state_render(state_t *state) {
  int fg = WHITE;
  int bg = BLACK;
  state_render_mode(state, fg, bg);
  for (int i = 0; i < MAX_NUM_STEPS; i++) {
    state_render_step(state, i, fg, bg);
    char cursor = (i == state->current_index) ? '>' : ' ';
    render_cursor_at(i, cursor, fg, bg);
  }
}

void render_splash(void) {
  display_clear(MAGENTA);
  display_text("purple", 20, 20, WHITE, BLACK, 1);
  display_text("earth", 30, 40, WHITE, BLACK, 1);
  display_text("hypoth-", 10, 60, WHITE, BLACK, 1);
  display_text("esis", 50, 80, WHITE, BLACK, 1);
}

void handle_command(command_t command, state_t *state) {
  int fg = WHITE;
  int bg = BLACK;
  switch (command.typ) {
    case COMMAND_HELLO: {
      printf("Hello, World!\n\r");
      break;
    }
    case COMMAND_SHOW_SPLASH: {
      render_splash();
      break;
    }
    case COMMAND_SHOW_UI: {
      display_clear(bg);
      state_render(state);
      break;
    }
    case COMMAND_SET_NOTE: {
      uint8_t note_index = command.args.set_note.note_index;
      char buf[4];
      sprintf(buf, "%s%d", note_name(note_index), note_octave(note_index));
      display_text(buf, 0, 0, fg, bg, 1);
      break;
    }
    case COMMAND_SET_SEQUENCE_INDEX: {
      uint8_t sequence_index = command.args.set_sequence_index.sequence_index;
      state_render_cursor(state, ' ', fg, bg);
      state->current_index = sequence_index;
      state_render_cursor(state, '>', fg, bg);
      break;
    }
    case COMMAND_SET_STEP_NOTE: {
      uint8_t sequence_index = command.args.set_step_note.sequence_index;
      uint8_t note_index = command.args.set_step_note.note_index;
      step_t *step = &state->sequence.steps[sequence_index];
      step->note_index = note_index;
      step->enabled = true;
      state_render_step(state, sequence_index, fg, bg);
      break;
    }
    case COMMAND_CLEAR_STEP_NOTE: {
      uint8_t sequence_index = command.args.set_step_note.sequence_index;
      step_t *step = &state->sequence.steps[sequence_index];
      step->enabled = false;
      state_render_step(state, sequence_index, fg, bg);
      break;
    }
    case COMMAND_SET_STEP_FLAGS: {
      uint8_t sequence_index = command.args.set_step_flags.sequence_index;
      uint8_t flags = command.args.set_step_flags.flags;
      step_t *step = &state->sequence.steps[sequence_index];
      step->flags = flags;
      state_render_step(state, sequence_index, fg, bg);
      break;
    }
    case COMMAND_CLEAR_SEQUENCE: {
      state_clear_sequence(state);
      state_render(state);
      break;
    }
    case COMMAND_SET_MODE: {
      state->mode = command.args.set_mode.mode;
      state_render_mode(state, fg, bg);
      break;
    }
    case COMMAND_SETTING_TEMPO: {
      state->setting_tempo = command.args.setting_tempo.setting_tempo;
      state_render_setting(state, fg, bg);
      break;
    }
    case COMMAND_SET_TEMPO: {
      state->tempo_bpm = command.args.set_tempo.tempo;
      state_render_setting(state, fg, bg);
      break;
    }
  }
}

state_t state;

int main(void) {
  timer2_init_pwm_port_d_bit_3(DISPLAY_BACKLIGHT_BRIGHTNESS);

  USART0_init();

  printf("UART initialized\n\r");

  display_init();

  state_init(&state);

  printf("Starting TWI event handler...\n\r");

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
