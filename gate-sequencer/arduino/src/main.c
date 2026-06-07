#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#include "uart_bitbanged.h"
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

inline uint16_t get_duty(void) {
  return ADC_read(ADC_CHANNEL_DUTY) + MIN_DUTY;
}

inline uint16_t get_tempo(void) {
  return ADC_read(ADC_CHANNEL_TEMPO);
}

uint32_t get_delay(void) {
  uint32_t tempo = (uint32_t)get_tempo() + 1;
  return 500000 / (tempo + 132); // (hand-tuned)
}

int main(void) {

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_bitbanged_init();

  // Initialize ADC pins
  ADC_init(ADC_CHANNEL_BIT_DUTY | ADC_CHANNEL_BIT_TEMPO);

  timer1_init();

  DDRC |= (PORTC_DECODER_ABCD | PORTC_DECODER_STROBE | PORTC_DECODER_INHIBIT);
  PORTC = 0;

  DDRB &= ~PORTB_RUN;
  PORTB |= PORTB_RUN;

  int8_t count = 0;

  while (1) {

    if ((PINB & PORTB_RUN) == 0) {
      continue;
    }

    uint16_t clock_delay = get_delay();
    uint16_t timer_value = timer1_read();
    bool tick_this_frame = timer_value > clock_delay;

    uint16_t duty = get_duty();

    if (timer_value < (uint16_t)(((uint32_t)duty * (uint32_t)clock_delay) / (uint32_t)MAX_DUTY)) {
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
    }
  }

  return 0;
}
