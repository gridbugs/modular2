#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include "util.h"
#include "key_matrix.h"

// Rows and columns here correspond to the layout of the board schematic - not
// the layout of the PCB. The strategy will be to "select" a column by driving
// its output pin low while keeping the other column's output pins high, then
// reading the state of the keys on each row of that column. Pressed keys will
// have a low voltage on their input pin and un-pressed keys will have a high
// voltage.

#define INPUT_ROW_PORTD_MIN 3
#define INPUT_ROW_PORTD_MAX 7
#define INPUT_ROW_PORTD_COUNT 5
#define INPUT_ROW_PORTD_MASK 0xF8
#define OUTPUT_COL_PORTB_MIN 0
#define OUTPUT_COL_PORTB_MAX 4
#define OUTPUT_COL_PORTB_MASK 0xF

void key_matrix_init(void) {
  // Data direction for input pins.
  DDRD &= ~INPUT_ROW_PORTD_MASK;
  // Internal pull-up resistor ofr input pins.
  PORTD |= INPUT_ROW_PORTD_MASK;
  // Data direction for output pins.
  DDRB |= OUTPUT_COL_PORTB_MASK;
  // Set all the output pins high (the deselected state).
  PORTB |= OUTPUT_COL_PORTB_MASK;
}

void key_matrix_scan(key_states_t* key_states) {
  key_states->prev = key_states->curr;
  key_states->curr = 0;
  for (int i = OUTPUT_COL_PORTB_MIN; i <= OUTPUT_COL_PORTB_MAX; i++) {
    // Drive the current column low.
    PORTB &= ~BIT(i);
    // Delay to give time for the output pins to change voltage and for the
    // input pins to detect the change.
    __asm__ __volatile__("nop");
    // Write 1s to the current state for each pressed key.
    key_states->curr |= ((uint32_t)(((uint8_t)(~PIND)) >> INPUT_ROW_PORTD_MIN) << (i * INPUT_ROW_PORTD_COUNT));
    PORTB |= BIT(i);
  }
}

typedef enum {
  KEY_INDEX_F1 = 4,
  KEY_INDEX_F2 = 9,
  KEY_INDEX_F3 = 14,
  KEY_INDEX_F4 = 19,
  KEY_INDEX_1 = 3,
  KEY_INDEX_2 = 8,
  KEY_INDEX_3 = 13,
  KEY_INDEX_4 = 18,
  KEY_INDEX_5 = 2,
  KEY_INDEX_6 = 7,
  KEY_INDEX_7 = 12,
  KEY_INDEX_8 = 17,
  KEY_INDEX_9 = 1,
  KEY_INDEX_10 = 6,
  KEY_INDEX_11 = 11,
  KEY_INDEX_12 = 16,
  KEY_INDEX_13 = 0,
  KEY_INDEX_14 = 5,
  KEY_INDEX_15 = 10,
  KEY_INDEX_16 = 15,
} key_index_t;

#define NUM_F_KEYS 4
key_index_t f_key_indices[NUM_F_KEYS] = { KEY_INDEX_F1, KEY_INDEX_F2, KEY_INDEX_F3, KEY_INDEX_F4 };

bool is_f_key_down(key_states_t *key_states, uint8_t f_key) {
  return (key_states->curr & BIT(f_key_indices[f_key])) != 0;
}

bool is_f_key_pressed_this_frame(key_states_t *key_states, uint8_t f_key) {
  return ((key_states->curr ^ key_states->prev) & key_states->curr & BIT(f_key_indices[f_key])) != 0;
}

#define KEY_INDEX_F_MASK (BIT(KEY_INDEX_F1) | BIT(KEY_INDEX_F2) | BIT(KEY_INDEX_F3) | BIT(KEY_INDEX_F4))

const int number_keys_by_key_index[20] = {
  [0] = 12,
  [1] = 8,
  [2] = 4,
  [3] = 0,
  [4] = -1,
  [5] = 13,
  [6] = 9,
  [7] = 5,
  [8] = 1,
  [9] = -1,
  [10] = 14,
  [11] = 10,
  [12] = 6,
  [13] = 2,
  [14] = -1,
  [15] = 15,
  [16] = 11,
  [17] = 7,
  [18] = 3,
  [19] = -1,
};

int get_down_number_key(key_states_t *key_states) {
  int trailing_zeros = __builtin_stdc_trailing_zeros(key_states->curr & ~KEY_INDEX_F_MASK);
  if (trailing_zeros == 32) {
    return -1;
  } else {
    return number_keys_by_key_index[trailing_zeros];
  }
}
