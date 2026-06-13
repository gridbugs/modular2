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

#define PORTB_RUN BIT(4)

#define ADC_CHANNEL_DUTY 6
#define ADC_CHANNEL_TEMPO 7
#define ADC_CHANNEL_BIT_DUTY BIT(ADC_CHANNEL_DUTY)
#define ADC_CHANNEL_BIT_TEMPO BIT(ADC_CHANNEL_TEMPO)

#define MIN_DUTY 127
#define MAX_DUTY (4095 + MIN_DUTY)

#define PORTD_CLOCK_SELECT_BIT BIT(0)
#define PORTD_CLOCK_BIT BIT(2)

inline uint16_t get_duty(void) {
  return ADC_read(ADC_CHANNEL_DUTY) + MIN_DUTY;
}

inline uint16_t get_tempo(void) {
  return ADC_read(ADC_CHANNEL_TEMPO);
}

inline bool clock_source_is_external(void) {
  return (PIND & PORTD_CLOCK_SELECT_BIT) == 0;
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
  //USART0_init();

  // Initialize ADC pins
  ADC_init(ADC_CHANNEL_BIT_DUTY | ADC_CHANNEL_BIT_TEMPO);

  timer1_init();

  DDRC |= (PORTC_DECODER_ABCD | PORTC_DECODER_STROBE | PORTC_DECODER_INHIBIT);
  PORTC = 0;

  DDRB &= ~PORTB_RUN;
  PORTB |= PORTB_RUN;

  int8_t count = 0;

  bool prev_external_clock = clock_source_is_external();
  set_clock_source(prev_external_clock);

  bool prev_external_clock_high = false;

  uint16_t prev_tick_duration = 0;

  while (1) {

    if ((PINB & PORTB_RUN) == 0) {
      continue;
    }

    bool external_clock = clock_source_is_external();

    // Handle change to the clock source
    if (external_clock != prev_external_clock) {
      set_clock_source(external_clock);
      prev_external_clock = external_clock;
    }

    uint16_t clock_delay = external_clock ? prev_tick_duration : get_delay();
    uint16_t timer_value = timer1_read();
    uint16_t duty = get_duty();
    bool gate_on = timer_value < (uint16_t)(((uint32_t)duty * (uint32_t)clock_delay) / (uint32_t)MAX_DUTY);

    bool tick_this_frame;
    if (external_clock) {
      bool external_clock_high = get_clock_in();
      tick_this_frame = external_clock_high && !prev_external_clock_high;
      prev_external_clock_high = external_clock_high;
    } else {
      tick_this_frame = timer_value > clock_delay;
      set_clock_out(gate_on);
    }

    if (gate_on) {
      PORTC &= ~PORTC_DECODER_INHIBIT;
    } else {
      PORTC |= PORTC_DECODER_INHIBIT;
    }

    if (tick_this_frame) {
      PORTC |= PORTC_DECODER_STROBE;
      PORTC &= ~PORTC_DECODER_ABCD;
      PORTC |= count & 0xF;
      PORTC &= ~PORTC_DECODER_STROBE;

      count++;
      timer1_reset();
      prev_tick_duration = timer_value;
    }
  }

  return 0;
}
