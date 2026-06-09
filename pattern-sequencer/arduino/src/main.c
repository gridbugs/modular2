#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include "util.h"
#include "uart.h"
#include "adc.h"
#include "timer.h"

#define PORTC_OUTPUT_SHIFT 2
#define PORTC_OUTPUT_MASK (0xF << PORTC_OUTPUT_SHIFT)

#define PORTC_CLOCK_BIT BIT(1)

#define PORTC_CLOCK_SELECT_BIT BIT(0)

#define PORTB_OUT_3_BIT BIT(5)
#define PORTD_OUT_2_BIT BIT(0)
#define PORTD_OUT_1_BIT BIT(1)

#define PORTD_BTN_1_BIT BIT(2)
#define PORTD_BTN_2_BIT BIT(3)
#define PORTD_BTN_3_BIT BIT(4)

#define PORTD_CLOCK_DIVIDER_BASE_SHIFT 5
#define PORTD_CLOCK_DIVIDER_BASE_0_BIT BIT(5)
#define PORTD_CLOCK_DIVIDER_BASE_1_BIT BIT(6)

#define PORTD_MODE_SELECT_BIT BIT(7)

#define PORTB_COUNT_MASK 0xF
#define PORTB_SHIFT_BIT BIT(4)

#define NUM_CHANNELS 3
#define MAX_NUM_STEPS 16

typedef enum {
  OUT_PORT_B,
  OUT_PORT_D,
} out_port_t;

typedef struct {
  out_port_t out_port;
  uint8_t out_port_bit;
  uint8_t btn_portd_bit;
  bool pressed_now;
  bool pressed_prev;
  bool sequence[MAX_NUM_STEPS];
} channel_t;

void set_count_leds(uint8_t count) {
  PORTB &= ~PORTB_COUNT_MASK;
  PORTB |= count;
}

void set_clock_divider(uint8_t value) {
  PORTC &= ~PORTC_OUTPUT_MASK;
  PORTC |= (value << PORTC_OUTPUT_SHIFT);
}

uint16_t get_delay_adc(void) {
  return ADC_read(6);
}

uint32_t get_delay(void) {
  uint32_t tempo_adc = (uint32_t)get_delay_adc() + 1;
  return 1082137 / (tempo_adc + 132); // (hand-tuned)
}

uint8_t get_num_steps(void) {
  uint16_t num_steps_adc = ((uint16_t)ADC_read(7));
  return (uint8_t)(num_steps_adc >> 8) + 1;
}

bool get_shift_button(void) {
  return (PINB & PORTB_SHIFT_BIT) == 0;
}

void set_clock_out(bool value) {
  if (value) {
    PORTC |= PORTC_CLOCK_BIT;
  } else {
    PORTC &= ~PORTC_CLOCK_BIT;
  }
}

bool get_clock_in(void) {
  return (PINC & PORTC_CLOCK_BIT) != 0;
}

bool clock_source_is_external(void) {
  return (PINC & PORTC_CLOCK_SELECT_BIT) == 0;
}

void set_clock_source(bool external) {
  // Update the data-direction bit for the clock pin
  if (external) {
    // Clock input pin
    DDRC &= ~PORTC_CLOCK_BIT;
  } else {
    // Clock output pin
    DDRC |= PORTC_CLOCK_BIT;
  }

}

uint8_t get_clock_divider_base(void) {
  uint8_t raw = (PIND & (PORTD_CLOCK_DIVIDER_BASE_0_BIT | PORTD_CLOCK_DIVIDER_BASE_1_BIT))
    >> PORTD_CLOCK_DIVIDER_BASE_SHIFT;
  switch (raw) {
    case 0:
      return 0;
    case 3:
      return 1;
    case 2:
      return 2;
  }
  return 0;
}

int main(void) {

  USART0_init();
  timer_init();

  // Initialize analog pins 6 and 7
  ADC_init(0xC0);


  // Initialize digital IO ports:
  DDRD |= (PORTD_OUT_2_BIT | PORTD_OUT_1_BIT);
  DDRD &= ~(PORTD_BTN_1_BIT |
      PORTD_BTN_2_BIT |
      PORTD_BTN_3_BIT |
      PORTD_CLOCK_DIVIDER_BASE_0_BIT |
      PORTD_CLOCK_DIVIDER_BASE_1_BIT |
      PORTD_MODE_SELECT_BIT);
  PORTD = (PORTD_BTN_1_BIT |
      PORTD_BTN_2_BIT |
      PORTD_BTN_3_BIT |
      PORTD_CLOCK_DIVIDER_BASE_0_BIT |
      PORTD_CLOCK_DIVIDER_BASE_1_BIT |
      PORTD_MODE_SELECT_BIT);

  DDRB |= (PORTB_COUNT_MASK | PORTB_OUT_3_BIT);
  DDRB &= ~PORTB_SHIFT_BIT;
  PORTB = PORTB_SHIFT_BIT;

  DDRC |= PORTC_OUTPUT_MASK;
  DDRC &= ~PORTC_CLOCK_SELECT_BIT;
  PORTC = PORTC_CLOCK_SELECT_BIT;

  channel_t channels[NUM_CHANNELS] = {
    [0] = {
      .out_port = OUT_PORT_D,
      .out_port_bit = PORTD_OUT_1_BIT,
      .btn_portd_bit = PORTD_BTN_1_BIT,
      .sequence = { 0 },
      .pressed_now = false,
      .pressed_prev = false,
    },
    [1] = {
      .out_port = OUT_PORT_D,
      .out_port_bit = PORTD_OUT_2_BIT,
      .btn_portd_bit = PORTD_BTN_2_BIT,
      .sequence = { 0 },
      .pressed_now = false,
      .pressed_prev = false,
    },
    [2] = {
      .out_port = OUT_PORT_B,
      .out_port_bit = PORTB_OUT_3_BIT,
      .btn_portd_bit = PORTD_BTN_3_BIT,
      .sequence = { 0 },
      .pressed_now = false,
      .pressed_prev = false,
    },
  };

  uint8_t count = 0;
  uint8_t divider_count = 0;
  uint8_t prev_num_steps = get_num_steps();
  bool prev_external_clock = clock_source_is_external();
  set_clock_source(prev_external_clock);
  timer_reset();
  uint16_t prev_period = 0;
  uint32_t clock_delay = get_delay();
  while (1) {
    bool external_clock = clock_source_is_external();

    // Handle change to the clock source
    if (external_clock != prev_external_clock) {
      set_clock_source(external_clock);
      prev_external_clock = external_clock;
    }

    bool tick_this_frame;
    uint16_t timer_value = timer_read();
    clock_delay = get_delay();
    tick_this_frame = timer_value > clock_delay;

    uint8_t num_steps = get_num_steps();
    if (prev_num_steps != num_steps) {
      prev_num_steps = num_steps;
    }

    if (tick_this_frame) {
      uint8_t prev_divider_count = divider_count;
      divider_count++;
      count++;
      if (count >= num_steps) {
        count = 0;
      }
      timer_reset();
      prev_period = timer_value;
      set_clock_out(true);
      set_count_leds(count);
      set_clock_divider((divider_count ^ prev_divider_count) >> get_clock_divider_base());
    } else {
      if (timer_value > (prev_period / 2)) {
        set_clock_out(false);
        set_clock_divider(0);
      }
    }

  }
  return 0;
}
