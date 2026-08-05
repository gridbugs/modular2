#include "state.h"

void state_init(state_t *state) {
  state->mode = MODE_PROGRAM;
  state->clock_source = CLOCK_SOURCE_EXTERNAL;
  state->clock_state = false;
  state->sequence.num_steps = MAX_NUM_STEPS;
  state->current_index = 0;
  state->tempo_bpm = 128;
  state->ticks_per_beat = 4;
  state->setting_tempo = false;
}

void state_add_to_current_index(state_t *state, int8_t delta) {
  int8_t new_current_index = (((int8_t)state->current_index) + delta);
  while (new_current_index < 0) {
    new_current_index += state->sequence.num_steps;
  }
  state->current_index = (uint8_t)(new_current_index % state->sequence.num_steps);
}

void state_clear_sequence(state_t *state) {
  state->current_index = 0;
  for (int i = 0; i < MAX_NUM_STEPS; i++) {
    step_t *step = &state->sequence.steps[i];
    step->enabled = false;
    step->flags = 0;
  }
}
