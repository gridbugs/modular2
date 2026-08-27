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
#include "note.h"
#include "note_indices.h"
#include "key_matrix.h"
#include "command.h"
#include "rotary_encoder.h"
#include "state.h"
#include "clock.h"
#include "debug.h"

#define PORTC_ENCODER_BUTTON_BIT BIT(0)
#define PORTC_ENCODER_A_BIT BIT(1)
#define PORTC_ENCODER_B_BIT BIT(2)
#define PORTC_CLOCK_BIT BIT(3)

#define PORTD_MODE_BIT BIT(0)
#define PORTD_GATE_BIT BIT(1)
#define PORTB_SCREEN_ARDUINO_RESET_BIT BIT(5)

static inline mode_t get_mode(void) {
  if ((PIND & PORTD_MODE_BIT) == 0) {
    return MODE_RUN;
  } else {
    return MODE_PROGRAM;
  }
}

// A volatile counter that will be updated asynchronously from the main control
// thread (by an ISR), and a copy that is expected to be atomically synchronized
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

static inline bool rotary_encoder_is_pressed(void) {
  return (PINC & PORTC_ENCODER_BUTTON_BIT) == 0;
}

ISR(PCINT1_vect) {
  uint8_t rotary_encoder_state = (PINC >> 1) & 3;
  int8_t direction = rotary_encoder_update(&rotary_encoder_history, rotary_encoder_state);
  if (direction != 0) {
    if (rotary_encoder_is_pressed()) {
      direction *= 8;
    }
    rotary_encoder_position.volatile_value += direction;
  }
}

static inline int8_t rotary_encoder_read_delta(void) {
  return async_counter_read_delta(&rotary_encoder_position);
}


void rotary_encoder_init(void) {
  // Enable pin-changed interrupts for PORTC
  PCICR |= BIT(PCIE1);

  // Allow pin-changed interrupts associated with rotary encoder.
  PCMSK1 |= PORTC_ENCODER_BUTTON_BIT | PORTC_ENCODER_A_BIT | PORTC_ENCODER_B_BIT;

  // Set pins connected to encoder as input pins.
  DDRC &= ~(PORTC_ENCODER_BUTTON_BIT | PORTC_ENCODER_A_BIT | PORTC_ENCODER_B_BIT);

  // Enable pull-up resistors for encoder pins.
  PORTC |= PORTC_ENCODER_BUTTON_BIT | PORTC_ENCODER_A_BIT | PORTC_ENCODER_B_BIT;
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

#define KEY_WITH_SHIFT_SET_TEMPO KEY_NOTE_C_1
#define KEY_WITH_SHIFT_SET_GATE KEY_NOTE_D_1

// This is a bit in the raw key matrix input, not the key_note_t type.
#define KEY_SHIFT_BIT BIT(0)
#define KEY_CLEAR_BIT BIT(6)

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
  command_buffer_push(cb, command_set_step_note(state->current_index, key_note));
}

void command_buffer_clear_note(command_buffer_t *cb, state_t *state) {
  step_t *step = state_current_step(state);
  step->enabled = false;
  command_buffer_push(cb, command_clear_step_note(state->current_index));
}

void command_buffer_clear_all(command_buffer_t *cb, state_t *state) {
  state_clear_sequence(state);
  command_buffer_push(cb, command_clear_sequence());
}

void command_buffer_toggle_flag(command_buffer_t *cb, state_t *state, uint8_t flag) {
  step_t *step = state_current_step(state);
  step->flags ^= flag;
  command_buffer_push(cb, command_set_step_flags(state->current_index, step->flags));
}

void command_buffer_set_mode(command_buffer_t *cb, state_t *state, mode_t mode) {
  state->mode = mode;
  command_buffer_push(cb, command_set_mode(mode));
}

void command_buffer_setting_tempo(command_buffer_t *cb, state_t *state, bool setting_tempo) {
  state->setting_tempo = setting_tempo;
  command_buffer_push(cb, command_setting_tempo(setting_tempo));
}

void command_buffer_setting_gate(command_buffer_t *cb, state_t *state, bool setting_gate) {
  state->setting_gate = setting_gate;
  command_buffer_push(cb, command_setting_gate(setting_gate));
}

void command_buffer_set_clock_source(command_buffer_t *cb, state_t *state, clock_source_t clock_source) {
  state->clock_source = clock_source;
  command_buffer_push(cb, command_set_clock_source(clock_source));
}

#define MIN_BPM 30
#define MAX_BPM 255

void command_buffer_add_to_tempo(command_buffer_t *cb, state_t *state, int8_t tempo_delta) {
  int16_t tempo_bpm = (int16_t)state->tempo_bpm + (int16_t)tempo_delta;
  state->tempo_bpm = tempo_bpm < MIN_BPM ? MIN_BPM : (tempo_bpm > MAX_BPM ? MAX_BPM : (uint8_t)tempo_bpm);
  command_buffer_push(cb, command_set_tempo(state->tempo_bpm));
}

void command_buffer_add_to_gate(command_buffer_t *cb, state_t *state, int8_t gate_delta) {
  int16_t gate_duration_ratio = (int16_t)state->gate_duration_ratio + (int16_t)gate_delta;
  state->gate_duration_ratio = gate_duration_ratio < 0 ? 0 : (gate_duration_ratio > 255 ? 255 : (uint8_t)gate_duration_ratio);
  command_buffer_push(cb, command_set_gate(state->gate_duration_ratio));
}

#define SECONDS_PER_MINUTE 60

uint16_t ticks_per_minute_to_timer_compare(uint16_t ticks_per_minute) {
  return (uint16_t)(((OSC_HZ >> 9) * SECONDS_PER_MINUTE) / (uint32_t)ticks_per_minute);
}

state_t state;
command_buffer_t command_buffer;
key_states_t key_states = { 0 };

typedef struct {
  volatile uint8_t count_setter_incremented;
  uint8_t count_to_check;
} async_flag_t;

void async_flag_set(async_flag_t *async_flag) {
  async_flag->count_setter_incremented++;
}

bool async_flag_check_and_clear(async_flag_t *async_flag) {
  uint8_t count_copy = async_flag->count_setter_incremented;
  bool ret = count_copy != async_flag->count_to_check;
  async_flag->count_to_check = count_copy;
  return ret;
}

async_flag_t timer_tick = { 0 };

ISR(TIMER1_COMPA_vect) {
  async_flag_set(&timer_tick);
}

volatile uint16_t timer2_match_count = 0;

ISR(TIMER2_COMPA_vect) {
  timer2_match_count++;
}

static inline uint16_t gate_timer_value(void) {
  return timer2_match_count;
}

static inline void gate_timer_reset(void) {
  timer2_match_count = 0;
}

void program_timer_ticks_per_minute(uint16_t ticks_per_minute) {
  // Divide the comparator by 2 so we get interrupts twice as often as the tick
  // rate. Each interrupt will toggle the state of the clock.
  uint16_t compare_value = ticks_per_minute_to_timer_compare(ticks_per_minute) / 2;
  timer1_set_output_compare_a(compare_value);
}

void set_clock_source(clock_source_t clock_source) {
  // Update the data-direction bit for the clock pin
  switch (clock_source) {
    case CLOCK_SOURCE_INTERNAL:
      // Clock output pin
      DDRC |= PORTC_CLOCK_BIT;
      break;
    case CLOCK_SOURCE_EXTERNAL:
      // Clock input pin
      DDRC &= ~PORTC_CLOCK_BIT;
      // Pull-up resistor for clock input pin
      PORTC |= PORTC_CLOCK_BIT;
      break;
  }
}

#define CLOCK_BOUNCE_THRESHOLD 100

bool get_clock_in(void) {
  static uint32_t since_change = 0;
  static bool state = false;
  bool raw = (PINC & PORTC_CLOCK_BIT) != 0;
  if (raw && !state && since_change >= CLOCK_BOUNCE_THRESHOLD) {
    state = true;
    since_change = 0;
  } else if (!raw && state && since_change >= CLOCK_BOUNCE_THRESHOLD) {
    state = false;
    since_change = 0;
  } else {
    since_change++;
  }
  return state;
}

void set_clock_out(bool value) {
  if (value) {
    PORTC |= PORTC_CLOCK_BIT;
  } else {
    PORTC &= ~PORTC_CLOCK_BIT;
  }
}

typedef enum {
  CLOCK_EVENT_NONE,
  CLOCK_EVENT_RISING_EDGE,
  CLOCK_EVENT_FALLING_EDGE,
} clock_event_t;

clock_event_t handle_clock(state_t *state) {
  switch (state->clock_source) {
    case CLOCK_SOURCE_EXTERNAL: {
      bool clock_state = get_clock_in();
      if (state->clock_state != clock_state) {
        state->clock_state = clock_state;
        if (clock_state) {
          return CLOCK_EVENT_RISING_EDGE;
        } else {
          return CLOCK_EVENT_FALLING_EDGE;
        }
      }
      break;
    }
    case CLOCK_SOURCE_INTERNAL: {
      if (async_flag_check_and_clear(&timer_tick)) {
        state->clock_state = !state->clock_state;
        set_clock_out(state->clock_state);
        if (state->clock_state) {
          return CLOCK_EVENT_RISING_EDGE;
        } else {
          return CLOCK_EVENT_FALLING_EDGE;
        }
      }
      break;
    }
  }
  return CLOCK_EVENT_NONE;
}

static inline void gate_on(void) {
  PORTD |= PORTD_GATE_BIT;
}

static inline void gate_off(void) {
  PORTD &= ~PORTD_GATE_BIT;
}

static inline void gate_init(void) {
  USART0_reset();
  DDRD |= PORTD_GATE_BIT;
  gate_off();
}

int main(void) {
#ifdef DEBUG_PRINTING
  // Allow printing over UART and defer initializing the gate (which shares the
  // TX pin) until after startup messages are printid.
  USART0_init();
#else
  gate_init();
#endif

  rotary_encoder_init();

  // Input pin with pullup resistor for mode switch.
  DDRD &= ~PORTD_MODE_BIT;
  PORTD |= PORTD_MODE_BIT;

  dprintf("Turning off screen arduino...\n\r");
  // Turn off the other arduino by driving its reset pin low
  DDRB |= PORTB_SCREEN_ARDUINO_RESET_BIT;
  PORTB &= ~PORTB_SCREEN_ARDUINO_RESET_BIT;

  COMPILER_BARRIER();

  // Wait some time to ensure the second arduino is fully off, then turn it  back on.
  delay_ms(50);

  COMPILER_BARRIER();

  dprintf("Turning on screen arduino...\n\r");
  PORTB |= PORTB_SCREEN_ARDUINO_RESET_BIT;

  dprintf("Waiting for screen arduino...\n\r");
  while (command_send(command_hello()) != 0);

  dprintf("Screen arduino is online!\n\r");
  delay_ms(50);

  // Display the splash screen.
  command_send(command_show_splash());
  delay_ms(500);

  dprintf("Starting UI...\n\r");
  command_send(command_show_ui());

  key_matrix_init();

  command_buffer_t command_buffer = { 0 };

  state_init(&state);

  set_clock_source(state.clock_source);

  timer1_init();
  timer1_enable_interrupt_output_compare_a();
  timer1_set_reset_on_output_compare_a_match();
  program_timer_ticks_per_minute(state_ticks_per_minute(&state));
  timer1_reset();
  timer1_start();

  sei();

  command_buffer_push(&command_buffer, command_set_note(current_note));
  dac0_set_value(note_dac_value((uint8_t)current_note));
  command_buffer_send(&command_buffer);

  timer2_init();
  timer2_set_output_compare_a(255);
  timer2_enable_interrupt_output_compare_a();
  timer2_start();
  uint16_t gate_timer_compare = 0;

#ifdef DEBUG_PRINTING
  // The gate is the TX pin. If printing is enabled then we can't initilaize
  // the gate until all init messages are printed.
  gate_init();
#endif

  while (1) {
    key_note_t new_current_note = current_note;
    step_t *current_step = state_current_step(&state);

    switch (handle_clock(&state)) {
      case CLOCK_EVENT_NONE:
        break;
      case CLOCK_EVENT_FALLING_EDGE:
        command_buffer_push(&command_buffer, command_set_clock(false));
        break;
      case CLOCK_EVENT_RISING_EDGE: {
        command_buffer_push(&command_buffer, command_set_clock(true));
        if (state.mode == MODE_RUN) {
          command_buffer_add_to_sequence_index(&command_buffer, &state, 1);
          if (current_step->enabled) {
            new_current_note = current_step->note_index;
          }
        }
        // Use the current tick duration to determine the next gate duration.
        gate_timer_compare = gate_timer_value();
        gate_timer_reset();
      }
    }

    uint16_t gate_timer_compare_scaled = (gate_timer_compare * (uint16_t)state.gate_duration_ratio) / 255;
    if (current_step->enabled && (gate_timer_value() < gate_timer_compare_scaled)) {
      gate_on();
    } else {
      gate_off();
    }

    mode_t mode = get_mode();
    if (mode != state.mode) {
      command_buffer_set_mode(&command_buffer, &state, mode);
    }

    key_matrix_scan(&key_states);
    uint32_t delta = key_states.curr ^ key_states.prev;
    uint32_t pressed = delta & key_states.curr;

    bool clear = (key_states.curr & KEY_CLEAR_BIT) != 0;
    bool shift = (key_states.curr & KEY_SHIFT_BIT) != 0;
    if (!shift) {
      if (state.setting_tempo) {
        command_buffer_setting_tempo(&command_buffer, &state, false);
      }
      if (state.setting_gate) {
        command_buffer_setting_gate(&command_buffer, &state, false);
      }
    }
    while (pressed) {
      int pressed_bit = __builtin_ctzl(pressed);
      pressed &= ~BIT(pressed_bit);
      key_t key = keys_by_key_matrix_bit[pressed_bit];
      if (key < KEY_NOTE_COUNT) {
        key_note_t key_note = (key_note_t)key;
        if (shift) {
          switch (key_note) {
            case KEY_WITH_SHIFT_SET_TEMPO:
              command_buffer_setting_tempo(&command_buffer, &state, true);
              break;
            case KEY_WITH_SHIFT_SET_GATE:
              command_buffer_setting_gate(&command_buffer, &state, true);
              break;
            default:
          }
        } else {
          note_stack[note_stack_size] = key_note;
          note_stack_size++;

          // Handle the fact that this key was just pressed
          command_buffer_press_note_key(&command_buffer, &state, key_note);
          command_buffer_add_to_sequence_index(&command_buffer, &state, 1);
        }
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
      } else if (key == KEY_CLOCK_SOURCE) {
        command_buffer_set_clock_source(&command_buffer, &state, CLOCK_SOURCE_INTERNAL);
        set_clock_source(CLOCK_SOURCE_INTERNAL);
      }
    }

    uint32_t released = delta & key_states.prev;
    while (released) {
      int released_bit = __builtin_ctzl(released);
      released &= ~BIT(released_bit);
      key_t key = keys_by_key_matrix_bit[released_bit];
      key_note_t key_note = (key_note_t)key;
      if (shift) {
        switch (key_note) {
          case KEY_WITH_SHIFT_SET_TEMPO:
            if (state.setting_tempo) {
              command_buffer_setting_tempo(&command_buffer, &state, false);
            }
            break;
          case KEY_WITH_SHIFT_SET_GATE:
            if (state.setting_gate) {
              command_buffer_setting_gate(&command_buffer, &state, false);
            }
            break;
          default:
        }
      }
      if (key == KEY_CLOCK_SOURCE) {
        command_buffer_set_clock_source(&command_buffer, &state, CLOCK_SOURCE_EXTERNAL);
        set_clock_source(CLOCK_SOURCE_EXTERNAL);
      }
      for (int i = 0; i < note_stack_size; i++) {
        if (note_stack[i] == key_note) {
          for (; i < note_stack_size - 1; i++) {
            note_stack[i] = note_stack[i + 1];
          }
          note_stack_size--;
          break;
        }
      }
    }

    int8_t rotary_encoder_delta = rotary_encoder_read_delta();
    if (rotary_encoder_delta != 0) {
      if (state.setting_tempo) {
        command_buffer_add_to_tempo(&command_buffer, &state, rotary_encoder_delta);
        program_timer_ticks_per_minute(state_ticks_per_minute(&state));
      } else if (state.setting_gate) {
        command_buffer_add_to_gate(&command_buffer, &state, rotary_encoder_delta);
      } else {
        // If the knob turns clockwise then clear the note before advancing the
        // cursor. If it turns anticlockwise then clear the note after
        // advancing the cursor. This is inconsistent but feels the least
        // incorrect in when actually using the feature in practice.
        if (clear && rotary_encoder_delta > 0) {
          command_buffer_clear_note(&command_buffer, &state);
        }
        command_buffer_add_to_sequence_index(&command_buffer, &state, rotary_encoder_delta);
        if (clear && rotary_encoder_delta < 0) {
          command_buffer_clear_note(&command_buffer, &state);
        }
      }
    }

    if (note_stack_size > 0) {
      new_current_note = note_stack[note_stack_size - 1];
    }

    if (new_current_note != current_note) {
      current_note = new_current_note;
      dac0_set_value(note_dac_value((uint8_t)current_note));
      command_buffer_push(&command_buffer, command_set_note(current_note));
    }

    command_buffer_send(&command_buffer);
  }

  return 0;
}
