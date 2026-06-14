#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint32_t curr;
  uint32_t prev;
} key_states_t;

void key_matrix_init(void);
void key_matrix_scan(key_states_t *key_states);

// f_key is the position of the f key, a number from 0..=3
bool is_f_key_down(key_states_t *key_states, uint8_t f_key);

// f_key is the position of the f key, a number from 0..=3
bool is_f_key_pressed_this_frame(key_states_t *key_states, uint8_t f_key);

// returns the numeric index of a currently down key (0..=15) or -1 if no number key is down
int get_down_number_key(key_states_t *key_states);
