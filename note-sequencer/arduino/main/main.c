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

#define DISPLAY_BACKLIGHT_BRIGHTNESS 0x20

#define PORTD_ENCODER_A BIT(7)
#define PORTD_ENCODER_B BIT(6)
#define PORTD_ENCODER_BUTTON_PIN BIT(5)
#define PORTD_BUTTON_RIGHT_PIN BIT(4)
#define PORTD_BUTTON_MIDDLE_PIN BIT(2)
#define PORTD_BUTTON_LEFT_PIN BIT(0)

// Leave space for TX (bit 1) and PWM (bit 3)
#define PORTD_INPUT_PINS ( \
    PORTD_ENCODER_A | \
    PORTD_ENCODER_B | \
    PORTD_ENCODER_BUTTON_PIN | \
    PORTD_BUTTON_RIGHT_PIN | \
    PORTD_BUTTON_MIDDLE_PIN | \
    PORTD_BUTTON_LEFT_PIN )

#define NUM_STEPS 16
#define VELOCITY_STEP_SIZE 32

volatile int sequence_index = 0;
volatile bool programming_mode = false;
volatile bool internal_clock = true;

// Unit is ms added to the iteration time of the main loop (which is pretty small!).
uint16_t internal_clock_delay = 100;

typedef enum {
  REST_01 = 0,
  REST_10 = 1,
} rotary_encoder_rest_t;

typedef enum {
  TURN_00 = 0,
  TURN_11 = 1,
  TURN_INIT = 2,
} rotary_encoder_turn_t;

typedef struct {
  rotary_encoder_rest_t last_rest;
  rotary_encoder_turn_t last_turn;
} rotary_encoder_history_t;

int8_t rotary_encoder_update(rotary_encoder_history_t *history, uint8_t current_state) {
  switch (current_state) {
    case 0:
      history->last_turn = TURN_00;
      break;
    case 1:
      if (history->last_rest == REST_10) {
        history->last_rest = REST_01;
        if (history->last_turn == TURN_00) {
          return -1;
        } else {
          return 1;
        }
      }
      break;
    case 2:
      if (history->last_rest == REST_01) {
        history->last_rest = REST_10;
        if (history->last_turn == TURN_00) {
          return 1;
        } else {
          return -1;
        }
      }
      break;
    case 3:
      history->last_turn = TURN_11;
      break;
  }
  return 0;
}

int16_t rotary_encoder_position = 0;
rotary_encoder_history_t rotary_encoder_history = {
  .last_rest = REST_01,
  .last_turn = TURN_INIT,
};

uint8_t prev_pind;

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

async_flag_t left_button = { 0 };
async_flag_t middle_button = { 0 };
async_flag_t right_button = { 0 };

volatile bool left_button_state = false;
volatile bool middle_button_state = false;
volatile bool right_button_state = false;

ISR(PCINT2_vect) {
  uint8_t pind = PIND;


  right_button_state = !(pind & PORTD_BUTTON_RIGHT_PIN);
  if ((prev_pind & PORTD_BUTTON_RIGHT_PIN) && right_button_state) {
    async_flag_set(&right_button);
    if (!programming_mode) {
      internal_clock = !internal_clock;
    }
  }
  if ((prev_pind & PORTD_BUTTON_MIDDLE_PIN) && !(pind & PORTD_BUTTON_MIDDLE_PIN)) {
    programming_mode = !programming_mode;
  }
  if ((prev_pind & PORTD_ENCODER_BUTTON_PIN) && !(pind & PORTD_ENCODER_BUTTON_PIN)) {
    if (programming_mode) {
      sequence_index = (sequence_index + 1) % NUM_STEPS;
    }
  }
  if ((prev_pind & PORTD_BUTTON_MIDDLE_PIN) && !(pind & PORTD_BUTTON_MIDDLE_PIN)) {
    async_flag_set(&middle_button);
  }
  if ((prev_pind & PORTD_BUTTON_LEFT_PIN) && !(pind & PORTD_BUTTON_LEFT_PIN)) {
    async_flag_set(&left_button);
  }

  uint8_t rotary_encoder_state = pind >> 6;
  int8_t rotary_encoder_delta = rotary_encoder_update(&rotary_encoder_history, rotary_encoder_state);
  rotary_encoder_position += (int16_t)rotary_encoder_delta;

  prev_pind = pind;
}

#define PORTC_GATE_PIN BIT(0)

void gate_on(void) {
  PORTC |= PORTC_GATE_PIN;
}

void gate_off(void) {
  PORTC &= ~PORTC_GATE_PIN;
}

#define PORTC_CLOCK_PIN BIT(1)

ISR(TIMER1_OVF_vect) {
  timer1_stop();
}

ISR(TIMER1_COMPA_vect) {
  gate_on();
}

ISR(TIMER1_COMPB_vect) {
  gate_off();
}

async_flag_t clock_rising_edge = { 0 };

uint8_t prev_pinc;

ISR(PCINT1_vect) {
  uint8_t pinc = PINC;
  if (!(prev_pinc & PORTC_CLOCK_PIN) && (pinc & PORTC_CLOCK_PIN)) {
    async_flag_set(&clock_rising_edge);
  }
  prev_pinc = pinc;
}

typedef struct {
  bool enabled;
  uint8_t note_index;
  uint8_t velocity;
} step_t;

step_t sequence[NUM_STEPS];

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

key_note_t note_stack[KEY_NOTE_COUNT] = {0};
uint8_t note_stack_size = 0 ;
key_note_t current_note = KEY_NOTE_C_1;

#define SECONDARY_ARDUINO_TWI_ADDRESS 0x42

int main(void) {

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_init();

  // Turn off the other arduino by driving its reset pin low
  DDRB |= BIT(5);
  PORTB &= ~BIT(5);

  timer2_init_pwm_port_d_bit_3(DISPLAY_BACKLIGHT_BRIGHTNESS);

  // Wait some time to ensure the second arduino is fully off, then turn it  back on.
  delay_ms(50);
  PORTB |= BIT(5);

  TWBR = 0;

  while (twi_send_byte(SECONDARY_ARDUINO_TWI_ADDRESS, 'x') != 0);

  key_matrix_init();
  key_states_t key_states = {0};

  while (1) {
    key_matrix_scan(&key_states);
    uint32_t delta = key_states.curr ^ key_states.prev;
    uint32_t pressed = delta & key_states.curr;
    while (pressed) {
      uint8_t pressed_bit = __builtin_stdc_trailing_zeros(pressed);
      pressed &= ~BIT(pressed_bit);
      key_t key = keys_by_key_matrix_bit[pressed_bit];
      if (key < KEY_NOTE_COUNT) {
        note_stack[note_stack_size] = (key_note_t)key;
        note_stack_size++;
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
      current_note = note_stack[note_stack_size - 1];
    }
    twi_send_byte(SECONDARY_ARDUINO_TWI_ADDRESS, current_note);
  }

  return 0;
}
