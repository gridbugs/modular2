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
#include "notes.h"
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

#define NOTE_DISPLAY_X 45
#define NOTE_DISPLAY_Y 55
#define NOTE_DISPLAY_FG WHITE
#define NOTE_DISPLAY_BG BLACK

typedef struct {
  bool enabled;
  note_t note;
  uint8_t note_index;
  uint8_t velocity;
} step_t;

step_t sequence[NUM_STEPS];

void step_set_note(step_t *step, uint8_t note_index) {
  step->note_index = note_index;
  note_t note = note_from_index(note_index);
  memcpy(&step->note, &note, sizeof(note_t));
}

void sequence_enable_step(int i, uint8_t note_index, uint8_t velocity) {
  step_t *step = &sequence[i];
  step->enabled = true;
  step->velocity = velocity;
  step_set_note(step, note_index);
}

void sequence_disable_step(int i) {
  sequence[i].enabled = false;
}

void init_sequence(void) {
  for (int i = 0; i < NUM_STEPS; i++) {
    sequence_enable_step(i, NOTE_C_3, 127);
    sequence_disable_step(i);
  }
}

char progress_buffer[6] = { 0 };
char status_buffer[3] = { 0 };

void display_current_note(void) {
  step_t step = sequence[sequence_index];
  uint16_t fg;
  if (step.enabled) {
    fg = WHITE;
  } else {
    fg = GREY;
  }
  display_text(step.note.name, NOTE_DISPLAY_X, NOTE_DISPLAY_Y, fg, NOTE_DISPLAY_BG, 1);
}

void display_current_velocity(void) {
  static char buf[4];
  step_t *step = &sequence[sequence_index];
  sprintf(buf, "%03d", step->velocity);
  display_text(buf, NOTE_DISPLAY_X, NOTE_DISPLAY_Y + 16, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);
}

void clear_velocity(void) {
  display_text("   ", NOTE_DISPLAY_X, NOTE_DISPLAY_Y + 16, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);
}

void display_bottom_line(void) {
  sprintf(progress_buffer, "%02d/%d", sequence_index, NUM_STEPS);
  display_text(progress_buffer, DISPLAY_WIDTH - 5 * 8, DISPLAY_HEIGHT - 8, BLACK, WHITE, 0);

  if (programming_mode) {
    status_buffer[0] = 'P';
  } else {
    status_buffer[0] = 'R';
  }
  if (internal_clock) {
    status_buffer[1] = 'I';
  } else {
    status_buffer[1] = 'E';
  }
  display_text(status_buffer, 0, DISPLAY_HEIGHT - 8, BLACK, WHITE, 0);
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

key_note_t note_stack[KEY_NOTE_COUNT] = {0};
uint8_t note_stack_size = 0 ;
key_note_t current_note = KEY_NOTE_C_1;

int main(void) {

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_init();
  printf("Hello, World!\n\r");

  // Turn off the other arduino by driving its reset pin low
  DDRB |= BIT(5);
  PORTB &= ~BIT(5);

  timer2_init_pwm_port_d_bit_3(DISPLAY_BACKLIGHT_BRIGHTNESS);

  // Wait some time to ensure the second arduino is fully off, then turn it  back on.
  delay_ms(50);
  PORTB |= BIT(5);

#define SECONDARY_ARDUINO_TWI_ADDRESS 0x42

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

  display_init();

  display_clear(MAGENTA);
  display_text("purple", 20, 10, WHITE, BLACK, 1);
  display_text("earth", 30, 30, WHITE, BLACK, 1);
  display_text("hypoth-", 10, 50, WHITE, BLACK, 1);
  display_text("esis", 50, 70, WHITE, BLACK, 1);

  sei();

  DDRD &= ~PORTD_INPUT_PINS;
  PORTD |= PORTD_INPUT_PINS;
  PCICR |= BIT(PCIE2) | BIT(PCIE1);
  PCMSK2 |= PORTD_INPUT_PINS;
  prev_pind = PIND;

  DDRC |= PORTC_GATE_PIN;
  DDRC &= ~PORTC_CLOCK_PIN;
  PCMSK1 |= PORTC_CLOCK_PIN;

  init_sequence();
  timer1_init();

  // Delay before setting the gate after changing notes to account for the
  // delay between sending the I2C message to change the DAC value and the
  // DAC's voltage actually changing.
  timer1_set_output_compare_a(1000);

  // Delay before turning the gate back off.
  timer1_set_output_compare_b(10000);

  printf("Hello, World!\n\r");

  display_clear(BLACK);

  // Initial sequence
  for (int i = 0; i < 2; i++) {
    int offset = i * 8;
    sequence_enable_step(offset + 0, NOTE_C_2, 127);
    sequence_disable_step(offset + 1);
    sequence_enable_step(offset + 2, NOTE_C_2, 127);
    sequence_disable_step(offset + 3);
    sequence_enable_step(offset + 4, NOTE_C_2, 255);
    sequence_disable_step(offset + 5);
    sequence_enable_step(offset + 6, NOTE_C_2, 127);
    sequence_disable_step(offset + 7);
  }


#ifdef INIT_TUNE
  sequence_disable_step(0);
  sequence_enable_step(1, NOTE_C_3, 127);
  sequence_enable_step(2, NOTE_C_4, 127);
  sequence_enable_step(3, NOTE_C_3, 255);
  sequence_enable_step(4, NOTE_D_SHARP_3, 127);
  sequence_enable_step(5, NOTE_F_SHARP_3, 127);
  sequence_enable_step(6, NOTE_A_3, 127);
  sequence_disable_step(7);
  sequence_enable_step(8, NOTE_C_4, 127);
  sequence_disable_step(9);
  sequence_enable_step(10, NOTE_F_SHARP_3, 127);
  sequence_enable_step(11, NOTE_A_3, 255);
  sequence_disable_step(12);
  sequence_enable_step(13, NOTE_D_SHARP_3, 127);
  sequence_disable_step(14);
  sequence_enable_step(15, NOTE_F_SHARP_3,  255);
#endif


  display_text("...", NOTE_DISPLAY_X, NOTE_DISPLAY_Y, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);
  int16_t prev_rotary_encoder_position = rotary_encoder_position;
  step_t *step = &sequence[sequence_index];
  bool prev_programming_mode = programming_mode;

  while (1) {
    if (programming_mode) {
      prev_programming_mode = true;
      step = &sequence[sequence_index];
      if (right_button_state) {
        int16_t velocity = step->velocity;
        if (rotary_encoder_position > prev_rotary_encoder_position) {
          velocity += VELOCITY_STEP_SIZE;
        } else if (rotary_encoder_position < prev_rotary_encoder_position) {
          velocity -= VELOCITY_STEP_SIZE;
        }
        step->velocity = (uint8_t)velocity;
      } else {
        if (rotary_encoder_position > prev_rotary_encoder_position) {
          step_set_note(step, step->note_index + 1);
        } else if (rotary_encoder_position < prev_rotary_encoder_position) {
          step_set_note(step, step->note_index - 1);
        }
      }
      if (async_flag_check_and_clear(&left_button)) {
        step->enabled = !step->enabled;
      }
      if (step->enabled) {
        dac0_set_value(step->note.dac_value);
        dac1_set_value(step->velocity << 4);
      } else {
        dac0_set_value(0);
        dac1_set_value(0);
      }
      prev_rotary_encoder_position = rotary_encoder_position;
      display_current_note();
      display_current_velocity();
      display_bottom_line();
    } else {
      if (prev_programming_mode) {
        clear_velocity();
      }
      prev_programming_mode = false;
      while (true) {
        if (rotary_encoder_position > prev_rotary_encoder_position) {
          step_set_note(step, step->note_index + 1);
          dac0_set_value(step->note.dac_value);
          dac1_set_value(step->velocity << 4);
        } else if (rotary_encoder_position < prev_rotary_encoder_position) {
          step_set_note(step, step->note_index - 1);
          dac0_set_value(step->note.dac_value);
          dac1_set_value(step->velocity << 4);
        }
        prev_rotary_encoder_position = rotary_encoder_position;

        if (async_flag_check_and_clear(&left_button)) {
          step->enabled = !step->enabled;
        }
        if (programming_mode) {
          break;
        } else if (internal_clock) {
          delay_ms(internal_clock_delay);
          break;
        } else if (async_flag_check_and_clear(&clock_rising_edge)) {
          break;
        }

      }
      if (programming_mode) {
        continue;
      }

      step = &sequence[sequence_index];


      if (step->enabled) {
        dac0_set_value(step->note.dac_value);
        dac1_set_value(step->velocity << 4);

        // Start the timer. A pair of compare interrupts will cause the gate to
        // be set and cleared. This makes it easy to add a delay before setting
        // the gate to account for a delay in the DAC output changinging, and to
        // clear the gate after a fixed period of time.
        timer1_reset_and_start();
      }

      display_current_note();
      display_bottom_line();

      sequence_index++;
      if (sequence_index == NUM_STEPS) {
        sequence_index = 0;
      }

    }
  }

  return 0;
}
