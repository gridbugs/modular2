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
#include "key_matrix.h"

#define PORTC_DECODER_ABCD (BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define PORTC_DECODER_INHIBIT BIT(4)
#define PORTC_DECODER_STROBE BIT(5)

#define PORTB_MODE1 BIT(4)
#define PORTD_MODE2 BIT(1)

#define ADC_CHANNEL_EXTRA 6
#define ADC_CHANNEL_TEMPO 7
#define ADT_CHANNEL_EXTRA BIT(ADC_CHANNEL_EXTRA)
#define ADC_CHANNEL_BIT_TEMPO BIT(ADC_CHANNEL_TEMPO)

#define MIN_DUTY 127
#define MAX_DUTY (4095 + MIN_DUTY)

#define PORTD_CLOCK_SELECT_BIT BIT(0)
#define PORTD_CLOCK_BIT BIT(2)

#define F_KEY_RESET 0
#define F_KEY_JUMP 1
#define F_KEY_SET_MIN_INDEX 2
#define F_KEY_SET_MAX_INDEX 3

uint32_t xorshift32_rand(void) {
  static uint32_t state = 0x12345678; // seed
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

inline uint16_t get_extra(void) {
  return ADC_read(ADC_CHANNEL_EXTRA);
}

inline uint16_t get_gate_duty(void) {
  return get_extra() + MIN_DUTY;
}

uint32_t get_gate_probability(void) {
  uint16_t extra = get_extra();
  if (extra >= 4090) {
    // Guarantee the gate if the knob is turned all the way (or most of the way).
    return 0xFFFFFFFF;
  } else {
    return ((uint32_t)extra) << 20;
  }
}

inline uint16_t get_tempo(void) {
  return ADC_read(ADC_CHANNEL_TEMPO);
}

inline bool clock_source_is_external(void) {
  return (PIND & PORTD_CLOCK_SELECT_BIT) == 0;
}

inline bool get_mode1(void) {
  return (PINB & PORTB_MODE1) == 0;
}

inline bool get_mode2(void) {
  return (PIND & PORTD_MODE2) == 0;
}

void set_clock_source(bool external) {
  // Update the data-direction bit for the clock pin
  if (external) {
    // Clock input pin
    DDRD &= ~PORTD_CLOCK_BIT;
  } else {
    // Clock output pin
    DDRD |= PORTD_CLOCK_BIT;
  }
}

bool get_clock_in(void) {
  return (PIND & PORTD_CLOCK_BIT) != 0;
}

void set_clock_out(bool value) {
  if (value) {
    PORTD |= PORTD_CLOCK_BIT;
  } else {
    PORTD &= ~PORTD_CLOCK_BIT;
  }
}

uint32_t get_delay(void) {
  uint32_t tempo = (uint32_t)get_tempo() + 1;
  return 500000 / (tempo + 132); // (hand-tuned)
}

int main(void) {

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_init();

  // Initialize ADC pins
  ADC_init(ADT_CHANNEL_EXTRA | ADC_CHANNEL_BIT_TEMPO);

  timer1_init();

  DDRC |= (PORTC_DECODER_ABCD | PORTC_DECODER_STROBE | PORTC_DECODER_INHIBIT);
  PORTC = 0;

  DDRB &= ~PORTB_MODE1;
  PORTB |= PORTB_MODE1;

  DDRD &= ~PORTD_MODE2;
  PORTD |= PORTD_MODE2;

  int8_t count = 0;

  bool prev_external_clock = clock_source_is_external();
  set_clock_source(prev_external_clock);

  bool prev_external_clock_high = false;

  uint16_t prev_tick_duration = 0;
  uint16_t gate_duty = 0xFFFF;
  uint32_t gate_probability = 0xFFFFFFFF;
  bool out_enabled = true;

  uint8_t min_index = 0;
  uint8_t max_index = 15;

  key_matrix_init();
  key_states_t key_states = { 0 };

  while (1) {

    bool external_clock = clock_source_is_external();

    // Handle change to the clock source
    if (external_clock != prev_external_clock) {
      set_clock_source(external_clock);
      prev_external_clock = external_clock;
      prev_tick_duration = 0;
    }

    uint16_t clock_delay = external_clock ? prev_tick_duration : get_delay();
    uint16_t timer_value = timer1_read();

    if (get_mode1()) {
      gate_duty = get_gate_duty();
      gate_probability = 0xFFFFFFFF;
    } else {
      gate_duty = 0xFFFF;
      gate_probability = get_gate_probability();
    }

    bool reverse = get_mode2();

    bool gate_on = timer_value < (uint16_t)(((uint32_t)gate_duty * (uint32_t)clock_delay) / (uint32_t)MAX_DUTY);

    bool tick_this_frame;
    if (external_clock) {
      bool external_clock_high = get_clock_in();
      tick_this_frame = external_clock_high && !prev_external_clock_high;
      prev_external_clock_high = external_clock_high;
    } else {
      tick_this_frame = timer_value > clock_delay;
      set_clock_out(gate_on);
    }

    key_matrix_scan(&key_states);
    int down_number_key = get_down_number_key(&key_states);
    if (down_number_key != -1) {
      if (is_f_key_down(&key_states, F_KEY_SET_MIN_INDEX)) {
        min_index = down_number_key;
        count = min_index;
      } else if (is_f_key_down(&key_states, F_KEY_SET_MAX_INDEX)) {
        max_index = down_number_key;
        count = max_index;
      } else if (is_f_key_down(&key_states, F_KEY_JUMP)) {
        count = down_number_key;
      } else {
        PORTC &= ~PORTC_DECODER_INHIBIT;
        PORTC |= PORTC_DECODER_STROBE;
        PORTC &= ~PORTC_DECODER_ABCD;
        PORTC |= down_number_key;
        PORTC &= ~PORTC_DECODER_STROBE;
        continue;
      }
    }

    if (is_f_key_pressed_this_frame(&key_states, 0)) {
      count = min_index;
    }

    // Clear the output if the gate is off.
    if (!gate_on || !out_enabled) {
      PORTC |= PORTC_DECODER_INHIBIT;
    }

    if (tick_this_frame) {
      out_enabled = xorshift32_rand() < gate_probability;

      if (down_number_key == -1) {
        if (out_enabled) {
          PORTC &= ~PORTC_DECODER_INHIBIT;
        } else {
          PORTC |= PORTC_DECODER_INHIBIT;
        }

        PORTC |= PORTC_DECODER_STROBE;
        PORTC &= ~PORTC_DECODER_ABCD;
        PORTC |= count & 0xF;
        PORTC &= ~PORTC_DECODER_STROBE;
      }

      if (reverse) {
        if (count == min_index) {
          count = max_index;
        } else if (count == 0) {
          count = 15;
        } else {
          count--;
        }
      } else {
        if (count == max_index) {
          count = min_index;
        } else if (count == 15) {
          count = 0;
        } else {
          count++;
        }
      }
      timer1_reset();
      prev_tick_duration = timer_value;
    }
  }

  return 0;
}
