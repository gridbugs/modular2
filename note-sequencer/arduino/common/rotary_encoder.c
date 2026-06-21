#include <stdint.h>
#include "rotary_encoder.h"

int8_t rotary_encoder_update(rotary_encoder_history_t *history, uint8_t current_state_ab) {
  switch (current_state_ab) {
    case 0:
      history->last_turn = TURN_00;
      break;
    case 1:
      if (history->last_rest == REST_10) {
        history->last_rest = REST_01;
        if (history->last_turn == TURN_00) {
          return 1;
        } else {
          return -1;
        }
      }
      break;
    case 2:
      if (history->last_rest == REST_01) {
        history->last_rest = REST_10;
        if (history->last_turn == TURN_00) {
          return -1;
        } else {
          return 1;
        }
      }
      break;
    case 3:
      history->last_turn = TURN_11;
      break;
  }
  return 0;
}
