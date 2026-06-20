#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint8_t note_index;
  bool enabled;
  bool accent;
  bool glide;
} step_t;

#define MAX_NUM_STEPS 16

typedef struct {
  step_t steps[MAX_NUM_STEPS];
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
