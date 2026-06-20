#include "state.h"

state_t state_new(void) {
  state_t state = { 0 };
  state.mode = MODE_PROGRAM;
  return state;
}
