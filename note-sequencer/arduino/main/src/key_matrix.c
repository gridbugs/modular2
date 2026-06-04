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

#define INPUT_ROW_PORTD_MIN 2
#define INPUT_ROW_PORTD_MAX 7
#define INPUT_ROW_PORTD_COUNT 6
#define INPUT_ROW_PORTD_MASK 0xFC
#define OUTPUT_COL_PORTB_MIN 0
#define OUTPUT_COL_PORTB_MAX 4
#define OUTPUT_COL_PORTB_MASK 0x1F

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
