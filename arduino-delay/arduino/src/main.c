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
#include "mcp4725.h"
#include "twi.h"

#define DELAY_ENABLE_PORT_B_BIT BIT(0)
#define DOWNSAMPLE_ENABLE_PORT_B_BIT BIT(1)
#define BITCRUSH_ENABLE_PORT_B_BIT BIT(2)

static inline bool is_delay_enabled(void) {
  return (PINB & DELAY_ENABLE_PORT_B_BIT) == 0;
}

static inline bool is_downsample_enabled(void) {
  return (PINB & DOWNSAMPLE_ENABLE_PORT_B_BIT) == 0;
}

static inline bool is_bitcrush_enabled(void) {
  return (PINB & BITCRUSH_ENABLE_PORT_B_BIT) == 0;
}

typedef struct rational {
  uint8_t numerator;
  uint8_t denominator_log_2;
} rational_t;

uint8_t rational_multiply(rational_t rational, uint8_t x) {
  return (uint8_t)((((uint16_t)x) * (uint16_t)rational.numerator)
      >> rational.denominator_log_2);
}

uint8_t rational_blend(rational_t out_ratio, uint8_t in, uint8_t out) {
  rational_t in_ratio = out_ratio;
  in_ratio.numerator = (1 << out_ratio.denominator_log_2) - out_ratio.numerator;
  return rational_multiply(out_ratio, out) + rational_multiply(in_ratio, in);
}

#define RAT(n, d) ((rational_t) { .numerator = n, .denominator_log_2 = d })

const rational_t feedback_scales[] = {
  [0] = RAT(0, 0),
  [1] = RAT(1, 2),
  [2] = RAT(1, 2),
  [3] = RAT(1, 1),
  [4] = RAT(1, 1),
  [5] = RAT(1, 1),
  [6] = RAT(3, 2),
  [7] = RAT(7, 3),
};

#define BUF_SIZE (1024 + 512)
uint8_t buf[BUF_SIZE] = {0};

static inline uint8_t get_input(void) {
  return (uint8_t)(ADC_read(7) >> 4);
}

static inline uint8_t get_cv(void) {
  return (uint8_t)(ADC_read(0) >> 4);
}

static inline uint16_t get_delay(void) {
  uint16_t raw = ADC_read(1);
  return (raw >> 2) + (raw >> 3);
}

static inline rational_t get_feedback(void) {
  uint16_t index = ADC_read(6) >> 10;
  return feedback_scales[index];
}

static inline uint8_t get_downsample(void) {
  return (uint8_t)(ADC_read(3) >> 4);
}

static inline uint8_t get_bitcrush(void) {
  return (uint8_t)(ADC_read(2) >> 4);
}

static inline uint16_t mod_bufsize(uint16_t x) {
  while (x >= BUF_SIZE) {
    x -= BUF_SIZE;
  }
  return x;
}

int main(void) {

  // Input pins with internal pull-up resistor
  DDRB &= DELAY_ENABLE_PORT_B_BIT | BITCRUSH_ENABLE_PORT_B_BIT | DOWNSAMPLE_ENABLE_PORT_B_BIT;
  PORTB |= DELAY_ENABLE_PORT_B_BIT | BITCRUSH_ENABLE_PORT_B_BIT | DOWNSAMPLE_ENABLE_PORT_B_BIT;

  ADC_init(BIT(7) | BIT(6));

  USART0_init();

  printf("Hello, World!\n\r");

  twi_transmit_start();
  twi_transmit_address(MCP4725_ADDR0, true);

  uint16_t input_index = 0;
  uint16_t delay = 0;
  uint16_t sample_countdown = 0;
  uint8_t input = get_input();

  while (true) {
    twi_transmit_data_start(MCP4725_COMMAND_UPDATE);
    uint16_t output_index = mod_bufsize(input_index + BUF_SIZE - delay);
    if (sample_countdown == 0) {
      input = get_input();
      sample_countdown = is_downsample_enabled() ? get_downsample() >> 3 : 0;
    } else {
      sample_countdown--;
    }
    uint8_t output = delay == 0 ? input : buf[output_index];
    if (is_bitcrush_enabled()) {
      uint8_t bitcrush = (get_bitcrush() >> 3) + 1;
      output = (output  / bitcrush) * bitcrush;
    }
    twi_transmit_data_end();
    twi_transmit_data_start(output);
    uint16_t output_with_feedback = is_delay_enabled() ? rational_blend(get_feedback(), input, output) : output;
    buf[input_index] = output_with_feedback;
    twi_transmit_data_end();
    twi_transmit_data_start(0);
    if (is_delay_enabled()) {
      delay = (uint16_t)get_delay() + (uint16_t)get_cv();
    } else {
      delay = 0;
    }
    input_index = mod_bufsize(input_index + 1);
    twi_transmit_data_end();
  }

  twi_transmit_stop();

  return 0;
}
