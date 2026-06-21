#pragma once

#include <stdint.h>

// When the enocder is stopped at a particular position its A, B pins will have
// different values (ie. 0, 1 or 1, 0).
typedef enum {
  REST_01,
  REST_10,
} rotary_encoder_rest_t;

// When the encoder is in between positions its A, B pins will have the same value
// (ie. 0, 0 or 1, 1). Also represents the case when the encoder has never been turned
// to prevent detecting a spurious turn when the device is first switched on.
typedef enum {
  TURN_00,
  TURN_11,
  TURN_INIT,
} rotary_encoder_turn_t;

// The previous rest and turn positions of the encoder. 
typedef struct {
  rotary_encoder_rest_t last_rest;
  rotary_encoder_turn_t last_turn;
} rotary_encoder_history_t;

#define ROTARY_ENCODER_HISTORY_INITIAL ((rotary_encoder_history_t) { \
    .last_rest = REST_01, \
    .last_turn = TURN_INIT, \
})

// Given the current state of the A, B rotary encoder pins and the previous
// rest and turn positions,
// return a -1 if the encoder was just turned to the left, a 1 if the encoder
// was just turned to the right, and a 0 otherwise.
int8_t rotary_encoder_update(rotary_encoder_history_t *history, uint8_t current_state_ab);
