#include "state.h"

void state_init(state_t *state) {
  state->mode = MODE_PROGRAM_OR_LIVE;
  state->clock_source = CLOCK_SOURCE_EXTERNAL;
  state->clock_state = false;
  state->sequence.num_steps = MAX_NUM_STEPS;
  state->playback_index = 0;
  state->edit_index = 0;
  state->tempo_bpm = 128;
  state->ticks_per_beat = 4;
  state->setting_tempo = false;
  state->setting_gate = false;
  state->setting_glide = false;
  state->gate_duration_ratio = 127;
  state->glide_duration_ratio = 127;
}

void state_add_to_playback_index(state_t *state, int8_t delta) {
  int8_t new_playback_index = (((int8_t)state->playback_index) + delta);
  while (new_playback_index < 0) {
    new_playback_index += state->sequence.num_steps;
  }
  state->playback_index = (uint8_t)(new_playback_index % state->sequence.num_steps);
}

void state_add_to_edit_index(state_t *state, int8_t delta) {
  int8_t new_edit_index = (((int8_t)state->edit_index) + delta);
  while (new_edit_index < 0) {
    new_edit_index += state->sequence.num_steps;
  }
  state->edit_index = (uint8_t)(new_edit_index % state->sequence.num_steps);
}


void state_clear_sequence(state_t *state) {
  state->edit_index = 0;
  state->playback_index = 0;
  for (int i = 0; i < MAX_NUM_STEPS; i++) {
    step_t *step = &state->sequence.steps[i];
    step->enabled = false;
    step->flags = 0;
  }
}
