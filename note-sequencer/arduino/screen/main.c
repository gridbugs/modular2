#include "command.h"
#include "display.h"
#include "note.h"
#include "note_indices.h"
#include "state.h"
#include "timer.h"
#include "twi.h"
#include "uart.h"
#include "util.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#define SEQUENCE_TOP_Y 24
#define MODE_LEFT_X 80

void state_render_mode(state_t *state, int fg, int bg) {
  char *text;
  switch (state->mode) {
  case MODE_RUN:
    text = " RUN  ";
    break;
  case MODE_PROGRAM_OR_LIVE:
    if (state->live) {
      text = "LIVE  ";
    } else {
      text = "PROG  ";
    }
    break;
  default:
    return;
  }
  display_text(text, MODE_LEFT_X, 120, fg, bg, 0);
}

void state_render_live_flags(state_t *state) {
  char accent_char = state_live_has_accent(state) ? 'A' : ' ';
  char glide_char = state_live_has_glide(state) ? 'G' : ' ';
  char buf[3] = {accent_char, glide_char, '\0'};
  display_text(buf, 112, 120, GREEN, BLACK, 0);
}

#define CLOCK_STATE_X 64

void state_render_clock_state(state_t *state, int fg, int bg) {
  char *text = state->clock_state ? "*" : " ";
  display_text(text, CLOCK_STATE_X, 120, fg, bg, 0);
}

void state_render_clock_source(state_t *state, int fg, int bg) {
  static char buf[16];
  char *source_name = state->clock_source == CLOCK_SOURCE_EXTERNAL ? "EXT" : "INT";
  sprintf(buf, "CLK: %s", source_name);
  display_text(buf, 0, 120, fg, bg, 0);
}

void state_render_edit_mode(state_t *state, int fg, int bg) {
  static char buf[16];
  char *mode_string = state->edit_mode == EDIT_MODE_STEP ? "STEP   " : "INPLACE";
  sprintf(buf, "EDT: %s", mode_string);
  display_text(buf, 0, 112, fg, bg, 0);
}

void state_render_accent_type(state_t *state, int fg, int bg) {
  static char buf[16];
  char *accent_string = "";
  switch (state->accent_type) {
  case ACCENT_TYPE_RISE:
    accent_string = "RISE";
    break;
  case ACCENT_TYPE_FLAT:
    accent_string = "FLAT";
    break;
  case ACCENT_TYPE_FALL:
    accent_string = "FALL";
    break;
  }
  sprintf(buf, "ACC: %s", accent_string);
  display_text(buf, 0, 104, fg, bg, 0);
}

void state_render_octave(state_t *state, int fg, int bg) {
  static char buf[2] = {0};
  display_text("OCT:", 88, 0, fg, bg, 0);
  display_text("OCT:", 88, 8, fg, bg, 0);
  buf[0] = '0' + state->octave;
  display_text(buf, 120, 0, fg, bg, 0);
  buf[0]++;
  display_text(buf, 120, 8, fg, bg, 0);
}

void render_cursor_at(int index, char cursor_char, int fg, int bg) {
  int cursor_x = ((index * 2) / MAX_NUM_STEPS) * 64;
  int cursor_y = SEQUENCE_TOP_Y + ((index % (MAX_NUM_STEPS / 2)) * 8);
  char buf[] = {cursor_char, '\0'};
  display_text(buf, cursor_x, cursor_y, fg, bg, 0);
}

void state_render_playback_cursor(state_t *state, char cursor_char, int fg, int bg) {
  render_cursor_at(state->playback_index, cursor_char, fg, bg);
}

void state_render_step(state_t *state, int step_index, int fg, int bg) {
  static char buf[32];
  int index_fg = GREY;
  if (state->edit_index == step_index) {
    bg = BLUE;
    index_fg = fg;
  }
  int x = ((step_index * 2) / MAX_NUM_STEPS) * 64;
  int y = SEQUENCE_TOP_Y + ((step_index % (MAX_NUM_STEPS / 2)) * 8);
  sprintf(buf, "%02d", step_index + 1);
  display_text(buf, x + 8, y, index_fg, bg, 0);
  step_t *step = &state->sequence.steps[step_index];
  if (step->enabled) {
    const char *name = note_name(step->note_index);
    uint8_t octave = note_octave(step->note_index);
    sprintf(buf, "%s%d", name, octave);
    display_text(buf, x + 24, y, fg, bg, 0);
  } else {
    display_text(" -  ", x + 24, y, fg, bg, 0);
  }
  char accent = step_has_accent(step) ? 'A' : ' ';
  char glide = step_has_glide(step) ? 'G' : ' ';
  int flag_fg = GREEN;
  sprintf(buf, "%c%c", accent, glide);
  display_text(buf, x + 48, y, flag_fg, bg, 0);
}

void state_render_setting(state_t *state, int fg, int bg) {
  static char buf[32];
  int y = 96;
  switch (state->setting_field) {
  case SETTING_FIELD_NONE:
    // Clear the screen region where the setting is displayed.
    sprintf(buf, "               ");
    break;
  case SETTING_FIELD_TEMPO_BPM:
    sprintf(buf, "TEMPO: %3u BPM", state->tempo_bpm);
    break;
  case SETTING_FIELD_GATE_DURATION_RATIO:
    sprintf(buf, "GATE: %3u/255 ", state->gate_duration_ratio);
    break;
  case SETTING_FIELD_GLIDE_DURATION_RATIO:
    sprintf(buf, "GLIDE: %3u/255", state->glide_duration_ratio);
    break;
  case SETTING_FIELD_ACCENT_DURATION_RATIO:
    sprintf(buf, "ACCDUR: %3u/255", state->accent_duration_ratio);
    break;
  case SETTING_FIELD_ACCENT_SIZE:
    sprintf(buf, "ACCSIZ: %3u/255", state->accent_size);
    break;
  }
  display_text(buf, 0, y, fg, bg, 0);
}

void state_render_edit_cursor(state_t *state) {
  state_render_step(state, state->edit_index, WHITE, BLUE);
}

void state_render(state_t *state) {
  int fg = WHITE;
  int bg = BLACK;
  state_render_mode(state, fg, bg);
  for (int i = 0; i < MAX_NUM_STEPS; i++) {
    state_render_step(state, i, fg, bg);
    char cursor = (i == state->playback_index) ? '>' : ' ';
    render_cursor_at(i, cursor, fg, bg);
  }
  state_render_clock_source(state, fg, bg);
  state_render_octave(state, fg, bg);
  state_render_accent_type(state, fg, bg);
  state_render_edit_mode(state, fg, bg);
}

void render_splash(void) {
  display_clear(MAGENTA);
  display_text("PURPLE", 20, 20, WHITE, BLACK, 1);
  display_text("EARTH", 30, 40, WHITE, BLACK, 1);
  display_text("HYPOTH-", 10, 60, WHITE, BLACK, 1);
  display_text("ESIS", 50, 80, WHITE, BLACK, 1);
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
  case COMMAND_SET_PLAYBACK_INDEX: {
    uint8_t sequence_index = command.args.set_playback_index.sequence_index;
    // Clear the playback cursor at its previous position.
    state_render_playback_cursor(state, ' ', fg, bg);
    state->playback_index = sequence_index;
    // Render the playback cursor at its current position.
    state_render_playback_cursor(state, '>', fg, bg);
    break;
  }
  case COMMAND_SET_EDIT_INDEX: {
    uint8_t sequence_index = command.args.set_edit_index.sequence_index;
    uint8_t edit_index_prev = state->edit_index;
    state->edit_index = sequence_index;
    // Clear the edit cursor at its previous position.
    state_render_step(state, edit_index_prev, WHITE, BLACK);
    // Render the edit cursor at its current position.
    state_render_step(state, state->edit_index, WHITE, BLACK);
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
  case COMMAND_SET_LIVE_FLAGS: {
    uint8_t flags = command.args.set_live_flags.flags;
    state->live_flags = flags;
    if (state->mode == MODE_PROGRAM_OR_LIVE && state->live) {
      state_render_live_flags(state);
    }
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
    if (state->mode == MODE_PROGRAM_OR_LIVE && state->live) {
      state_render_live_flags(state);
    }
    break;
  }
  case COMMAND_SET_LIVE: {
    state->live = command.args.set_live.live;
    state_render_mode(state, fg, bg);
    if (state->mode == MODE_PROGRAM_OR_LIVE && state->live) {
      state_render_live_flags(state);
    }
    break;
  }
  case COMMAND_SETTING_FIELD: {
    state->setting_field = command.args.setting_field.setting_field;
    state_render_setting(state, fg, bg);
    break;
  }
  case COMMAND_SET_TEMPO: {
    state->tempo_bpm = command.args.set_tempo.tempo;
    state_render_setting(state, fg, bg);
    break;
  }
  case COMMAND_SET_GATE: {
    state->gate_duration_ratio = command.args.set_gate.gate;
    state_render_setting(state, fg, bg);
    break;
  }
  case COMMAND_SET_GLIDE: {
    state->glide_duration_ratio = command.args.set_glide.glide;
    state_render_setting(state, fg, bg);
    break;
  }
  case COMMAND_SET_CLOCK: {
    state->clock_state = command.args.set_clock.clock_state;
    state_render_clock_state(state, fg, bg);
    break;
  }
  case COMMAND_SET_CLOCK_SOURCE: {
    state->clock_source = command.args.set_clock_source.clock_source;
    state_render_clock_source(state, fg, bg);
    break;
  }
  case COMMAND_SET_OCTAVE: {
    state->octave = command.args.set_octave.octave;
    state_render_octave(state, fg, bg);
    break;
  }
  case COMMAND_SET_EDIT_MODE: {
    state->edit_mode = command.args.set_edit_mode.edit_mode;
    state_render_edit_mode(state, fg, bg);
    break;
  }
  case COMMAND_SET_ACCENT_TYPE: {
    state->accent_type = command.args.set_accent_type.accent_type;
    state_render_accent_type(state, fg, bg);
    break;
  }
  case COMMAND_SET_ACCENT_DURATION_RATIO: {
    state->accent_duration_ratio = command.args.set_accent_duration_ratio.accent_duration_ratio;
    state_render_setting(state, fg, bg);
    break;
  }
  case COMMAND_SET_ACCENT_SIZE: {
    state->accent_size = command.args.set_accent_size.accent_size;
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

  while (1) {
    while (command_ring_prev_read_index < command_ring_next_write_index) {
      handle_command(command_ring[command_ring_prev_read_index % COMMAND_RING_SIZE], &state);
      command_ring_prev_read_index++;
    }
  }

  return 0;
}
