#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "util.h"

#define FLAG_ACCENT BIT(0)
#define FLAG_GLIDE BIT(1)

typedef struct {
  uint8_t note_index;
  bool enabled;
  uint8_t flags;
} step_t;

static inline bool step_has_accent(step_t *step) {
  return (step->flags & FLAG_ACCENT) != 0;
}

static inline bool step_has_glide(step_t *step) {
  return (step->flags & FLAG_GLIDE) != 0;
}

#define MAX_NUM_STEPS 16

typedef struct {
  step_t steps[MAX_NUM_STEPS];
  uint8_t num_steps;
} sequence_t;

typedef enum {
  MODE_RUN,
  MODE_PROGRAM,
} mode_t;

typedef struct {
  sequence_t sequence;
  uint8_t current_index;
  mode_t mode;
} state_t;

state_t state_new(void);
void state_add_to_current_index(state_t *state, int8_t delta);

static inline step_t *state_current_step(state_t *state) {
  return &state->sequence.steps[state->current_index];
}
