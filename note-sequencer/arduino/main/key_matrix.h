#include <stdint.h>

#define MAX_NUM_KEYS 32
#define DEBOUNCE_PERIOD 100

typedef struct {
  uint32_t curr;
  uint32_t prev;
  uint8_t debounce_counter_per_key[MAX_NUM_KEYS];
} key_states_t;

void key_matrix_init(void);
void key_matrix_scan(key_states_t* key_states);
