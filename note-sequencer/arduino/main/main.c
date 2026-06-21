#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#include "uart.h"
#include "adc.h"
#include "timer.h"
#include "twi.h"
#include "mcp4725.h"
#include "display.h"
#include "note.h"
#include "note_indices.h"
#include "key_matrix.h"
#include "command.h"
#include "rotary_encoder.h"
#include "state.h"

#define PORTC_ENCODER_BUTTON BIT(0)
#define PORTC_ENCODER_A BIT(1)
#define PORTC_ENCODER_B BIT(2)

// A volatile counter that will be updated asynchronously from the main control
// thread (by an ISR), and a copy that is expected tobe atomically synchronized
// with the volatile counter by the main control thread. It's assumed that the
// counters will never overflow.
typedef struct {
  volatile int32_t volatile_value;
  int32_t value;
} async_counter_t;

// Returns the change in the counter value since the last time it was read (by
// calling this function).
int8_t async_counter_read_delta(async_counter_t *async_counter) {
  // Atomically read the volatile value by reading it with interrupts disabled.
  cli();
  int32_t volatile_value = async_counter->volatile_value;
  sei();
  int32_t delta = volatile_value - async_counter->value;
  async_counter->value = volatile_value;

  // Clamp the delta so it fits in an int8_t. The use case is for a rotary
  // encoder, and the delta exceeding the int8_t range would mean the encoder
  // has been turned +/-128 positions since the last time its value was
  // checked, which a) is very unlikely and b) would mean the consequence of a
  // clamped value being used in place of the true value is negligable.
  if (delta > 127) {
    delta = 127;
  } else if (delta < -128) {
    delta = -128;
  }

  return (int8_t)delta;
}

async_counter_t rotary_encoder_position = { 0 };
rotary_encoder_history_t rotary_encoder_history = ROTARY_ENCODER_HISTORY_INITIAL;

ISR(PCINT1_vect) {
  uint8_t rotary_encoder_state = (PINC >> 1) & 3;
  int8_t direction = rotary_encoder_update(&rotary_encoder_history, rotary_encoder_state);
  if (direction != 0) {
    rotary_encoder_position.volatile_value += direction;
  }
}

inline int8_t rotary_encoder_read_delta(void) {
  return async_counter_read_delta(&rotary_encoder_position);
}

inline bool rotary_encoder_pressed(void) {
  return (PINC & PORTC_ENCODER_BUTTON) == 0;
}

void rotary_encoder_init(void) {
  // Enable pin-changed interrupts for PORTC
  PCICR |= BIT(PCIE1);

  // Allow pin-changed interrupts associated with rotary encoder.
  PCMSK1 |= PORTC_ENCODER_BUTTON | PORTC_ENCODER_A | PORTC_ENCODER_B;

  // Set pins connected to encoder as input pins.
  DDRC &= ~(PORTC_ENCODER_BUTTON | PORTC_ENCODER_A | PORTC_ENCODER_B);

  // Enable pull-up resistors for encoder pins.
  PORTC |= PORTC_ENCODER_BUTTON | PORTC_ENCODER_A | PORTC_ENCODER_B;
}

typedef enum {
  KEY_C_1,
  KEY_C_SHARP_1,
  KEY_D_1,
  KEY_D_SHARP_1,
  KEY_E_1,
  KEY_F_1,
  KEY_F_SHARP_1,
  KEY_G_1,
  KEY_G_SHARP_1,
  KEY_A_1,
  KEY_A_SHARP_1,
  KEY_B_1,
  KEY_C_2,
  KEY_C_SHARP_2,
  KEY_D_2,
  KEY_D_SHARP_2,
  KEY_E_2,
  KEY_F_2,
  KEY_F_SHARP_2,
  KEY_G_2,
  KEY_G_SHARP_2,
  KEY_A_2,
  KEY_A_SHARP_2,
  KEY_B_2,
  KEY_C_3,
  KEY_X_1,
  KEY_X_2,
  KEY_X_3,
  KEY_X_4,
  KEY_CLOCK_SOURCE,
} key_t;

const key_t keys_by_key_matrix_bit[] = {
  KEY_X_1,
  KEY_G_SHARP_2,
  KEY_D_SHARP_2,
  KEY_A_SHARP_1,
  KEY_F_1,
  KEY_C_1,
  KEY_X_2,
  KEY_A_2,
  KEY_E_2,
  KEY_B_1,
  KEY_F_SHARP_1,
  KEY_C_SHARP_1,
  KEY_X_3,
  KEY_A_SHARP_2,
  KEY_F_2,
  KEY_C_2,
  KEY_G_1,
  KEY_D_1,
  KEY_X_4,
  KEY_B_2,
  KEY_F_SHARP_2,
  KEY_C_SHARP_2,
  KEY_G_SHARP_1,
  KEY_D_SHARP_1,
  KEY_CLOCK_SOURCE,
  KEY_C_3,
  KEY_G_2,
  KEY_D_2,
  KEY_A_1,
  KEY_E_1,
};

typedef enum {
  KEY_NOTE_C_1,
  KEY_NOTE_C_SHARP_1,
  KEY_NOTE_D_1,
  KEY_NOTE_D_SHARP_1,
  KEY_NOTE_E_1,
  KEY_NOTE_F_1,
  KEY_NOTE_F_SHARP_1,
  KEY_NOTE_G_1,
  KEY_NOTE_G_SHARP_1,
  KEY_NOTE_A_1,
  KEY_NOTE_A_SHARP_1,
  KEY_NOTE_B_1,
  KEY_NOTE_C_2,
  KEY_NOTE_C_SHARP_2,
  KEY_NOTE_D_2,
  KEY_NOTE_D_SHARP_2,
  KEY_NOTE_E_2,
  KEY_NOTE_F_2,
  KEY_NOTE_F_SHARP_2,
  KEY_NOTE_G_2,
  KEY_NOTE_G_SHARP_2,
  KEY_NOTE_A_2,
  KEY_NOTE_A_SHARP_2,
  KEY_NOTE_B_2,
  KEY_NOTE_C_3,
} key_note_t;

#define KEY_NOTE_COUNT 25
#define KEY_CLEAR KEY_X_2
#define KEY_ACCENT KEY_X_3
#define KEY_GLIDE KEY_X_4

// This is a bit in the raw key matrix input, not the key_note_t type.
#define KEY_SHIFT_BIT BIT(0)

key_note_t note_stack[KEY_NOTE_COUNT] = {0};
uint8_t note_stack_size = 0 ;
key_note_t current_note = KEY_NOTE_C_1;

typedef struct {
  command_t commands[32];
  uint8_t num_commands;
} command_buffer_t;

void command_buffer_send(command_buffer_t *cb) {
  if (cb->num_commands > 0) {
    commands_send(cb->commands, cb->num_commands);
    cb->num_commands = 0;
  }
}

void command_buffer_push(command_buffer_t *cb, command_t command) {
  cb->commands[cb->num_commands] = command;
  cb->num_commands++;
}

void command_buffer_add_to_sequence_index(command_buffer_t *cb, state_t *state, int8_t delta) {
  state_add_to_current_index(state, delta);
  command_buffer_push(cb, command_set_sequence_index(state->current_index));
}

void command_buffer_press_note_key(command_buffer_t *cb, state_t *state, key_note_t key_note) {
  step_t *step = state_current_step(state);
  step->note_index = key_note;
  step->enabled = true;
  command_buffer_push(cb, command_set_sequence_note(state->current_index, key_note));
}

void command_buffer_clear_note(command_buffer_t *cb, state_t *state) {
  step_t *step = state_current_step(state);
  step->enabled = false;
  command_buffer_push(cb, command_clear_sequence_note(state->current_index));
}

// This function has the side effect of sending buffers of commands to save on buffer size.
void command_buffer_clear_all(command_buffer_t *cb, state_t *state) {
  state->current_index = 0;
  command_buffer_push(cb, command_set_sequence_index(0));
  for (int i = 0; i < MAX_NUM_STEPS / 2; i++) {
    step_t *step = &state->sequence.steps[i];
    step->enabled = false;
    step->flags = 0;
    command_buffer_push(cb, command_clear_sequence_note(i));
    command_buffer_push(cb, command_set_step_flags(i, 0));
  }
  command_buffer_send(cb);
  delay_ms(10);
  for (int i = MAX_NUM_STEPS / 2; i < MAX_NUM_STEPS; i++) {
    step_t *step = &state->sequence.steps[i];
    step->flags = 0;
    command_buffer_push(cb, command_clear_sequence_note(i));
    command_buffer_push(cb, command_set_step_flags(i, 0));
  }
}

void command_buffer_toggle_flag(command_buffer_t *cb, state_t *state, uint8_t flag) {
  step_t *step = state_current_step(state);
  step->flags ^= flag;
  command_buffer_push(cb, command_set_step_flags(state->current_index, step->flags));
}

int main(void) {

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_init();

  rotary_encoder_init();

  // Turn off the other arduino by driving its reset pin low
  DDRB |= BIT(5);
  PORTB &= ~BIT(5);

  // Wait some time to ensure the second arduino is fully off, then turn it  back on.
  delay_ms(50);
  PORTB |= BIT(5);

  printf("Waiting for screen arduino...\n\r");
  while (command_send(command_hello()) != 0);

  printf("Screen arduino is online!\n\r");

  // Display the splash screen.
  command_send(command_show_splash());
  delay_ms(1000);
  command_send(command_show_ui());

  key_matrix_init();
  key_states_t key_states = {0};

  command_buffer_t command_buffer;

  state_t state = state_new();

  sei();

  while (1) {
    int8_t rotary_encoder_delta = rotary_encoder_read_delta();
    if (rotary_encoder_delta != 0) {
      command_buffer_add_to_sequence_index(&command_buffer, &state, rotary_encoder_delta);
    }
    key_matrix_scan(&key_states);
    uint32_t delta = key_states.curr ^ key_states.prev;
    uint32_t pressed = delta & key_states.curr;
    bool shift = (key_states.curr & KEY_SHIFT_BIT) != 0;
    while (pressed) {
      uint8_t pressed_bit = __builtin_stdc_trailing_zeros(pressed);
      pressed &= ~BIT(pressed_bit);
      key_t key = keys_by_key_matrix_bit[pressed_bit];
      if (key < KEY_NOTE_COUNT) {
        key_note_t key_note = (key_note_t)key;
        note_stack[note_stack_size] = key_note;
        note_stack_size++;

        // Handle the fact that this key was just pressed
        command_buffer_press_note_key(&command_buffer, &state, key_note);
        command_buffer_add_to_sequence_index(&command_buffer, &state, 1);
      } else if (key == KEY_CLEAR) {
        if (shift) {
          command_buffer_clear_all(&command_buffer, &state);
        } else {
          // Clear the current step
          command_buffer_clear_note(&command_buffer, &state);
          command_buffer_add_to_sequence_index(&command_buffer, &state, 1);
        }
      } else if (key == KEY_ACCENT) {
        command_buffer_toggle_flag(&command_buffer, &state, FLAG_ACCENT);
      } else if (key == KEY_GLIDE) {
        command_buffer_toggle_flag(&command_buffer, &state, FLAG_GLIDE);
      }
    }
    uint32_t released = delta & key_states.prev;
    while (released) {
      uint8_t released_bit = __builtin_stdc_trailing_zeros(released);
      released &= ~BIT(released_bit);
      key_t key = keys_by_key_matrix_bit[released_bit];
      for (int i = 0; i < note_stack_size; i++) {
        if (note_stack[i] == (key_note_t)key) {
          for (; i < note_stack_size - 1; i++) {
            note_stack[i] = note_stack[i + 1];
          }
          note_stack_size--;
          break;
        }
      }
    }
    if (note_stack_size > 0) {
      key_note_t new_current_note = note_stack[note_stack_size - 1];
      if (new_current_note != current_note) {
        command_buffer_push(&command_buffer, command_set_note(new_current_note));
        current_note = new_current_note;
      }
    }

    command_buffer_send(&command_buffer);
  }

  return 0;
}
