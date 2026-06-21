#include "state.h"

state_t state_new(void) {
  state_t state = { 0 };
  state.mode = MODE_PROGRAM;
  state.sequence.num_steps = MAX_NUM_STEPS;
  return state;
}

void state_add_to_current_index(state_t *state, int8_t delta) {
  int8_t new_current_index = (((int8_t)state->current_index) + delta);
  while (new_current_index < 0) {
    new_current_index += state->sequence.num_steps;
  }
  state->current_index = (uint8_t)(new_current_index % state->sequence.num_steps);
}
