#include <stdint.h>

typedef struct {
  uint32_t curr;
  uint32_t prev;
} key_states_t;

void key_matrix_init(void);
void key_matrix_scan(key_states_t* key_states);
