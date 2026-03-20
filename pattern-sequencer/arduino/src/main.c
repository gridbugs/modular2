#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include "util.h"
#include "uart_bitbanged.h"
#include "adc.h"
#include "timer.h"

#define COUNT_MASK (BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4))

#define NUM_CHANNELS 4
#define MAX_NUM_STEPS 32

typedef struct {
  uint8_t adc_index;
  uint8_t out_portb_index;
  bool pressed_now;
  bool pressed_prev;
  bool allow_fill;
  bool sequence[MAX_NUM_STEPS];
} channel_t;

void set_count_leds(uint8_t count) {
  uint8_t count_bits =
    ((count & BIT(0)) << 4) |
    ((count & BIT(1)) << 2) |
    (count & BIT(2)) |
    ((count & BIT(3)) >> 2) |
    ((count & BIT(4)) >> 4);
  PORTD &= ~COUNT_MASK;
  PORTD |= count_bits;
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
  return (uint8_t)(num_steps_adc >> 7) + 1;
}

bool get_shift_button(void) {
  return (PINB & BIT(5)) == 0;
}

void set_clock_out(bool value) {
  if (value) {
    PORTC |= BIT(0);
  } else {
    PORTC &= ~BIT(0);
  }
}

bool get_clock_in(void) {
  return (PINC & BIT(0)) != 0;
}

bool clock_source_is_external(void) {
  return (PINC & BIT(1)) == 0;
}

void set_clock_source(bool external) {
  // Update the data-direction bit for the clock pin
  if (external) {
    // Clock input pin
    DDRC &= ~BIT(0);
  } else {
    // Clock output pin
    DDRC |= BIT(0);
  }

}

#define MIN_GATE 8
#define NUM_STEPS_DISPLAY_DELAY 2000
#define SHIFT_DOUBLE_TAP_WINDOW 500

int main(void) {
  timer_init();

  // Initializing analog pins, leaving the bottom 2 for use as digital IO pins.
  ADC_init(0xFC);

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_bitbanged_init();

  // Initialize digital IO ports:

  // Output pins for counter
  DDRD |= COUNT_MASK;

  // Input pins for all 5 buttons
  DDRD &= ~(BIT(5) | BIT(6) | BIT(7));
  DDRB &= ~(BIT(0) | BIT(5));

  // Output pins for LEDs showing the sequence
  DDRB |= BIT(1) | BIT(2) | BIT(3) | BIT(4);

  // Clock output pin
  DDRC |= BIT(0);

  // Clock select input pin
  DDRC &= ~BIT(1);

  // Enable internal pull-up resistor for IO pins used for buttons.
  PORTD |= BIT(5) | BIT(6) | BIT(7);
  PORTB |= BIT(0) | BIT(5);

  // Clock select pull-up resistor
  PORTC |= BIT(1);

  // Wait for debouncing capacitors to charge.
  for (long int i = 0; i < 1000000; i++);

  channel_t channels[NUM_CHANNELS] = {
    [0] = {
      .adc_index = 2,
      .out_portb_index = 4,
      .sequence = { 0 },
      .pressed_now = false,
      .pressed_prev = false,
      .allow_fill = true,
    },
    [1] = {
      .adc_index = 3,
      .out_portb_index = 3,
      .sequence = { 0 },
      .pressed_now = false,
      .pressed_prev = false,
      .allow_fill = false,
    },
    [2] = {
      .adc_index = 4,
      .out_portb_index = 2,
      .sequence = { 0 },
      .pressed_now = false,
      .pressed_prev = false,
      .allow_fill = false,
    },
    [3] = {
      .adc_index = 5,
      .out_portb_index = 1,
      .sequence = { 0 },
      .pressed_now = false,
      .pressed_prev = false,
      .allow_fill = false,
    },
  };

  uint8_t count = 0;
  bool reverse = true;
  uint8_t prev_num_steps = get_num_steps();
  uint32_t num_steps_display_countdown = 0;
  bool prev_external_clock = clock_source_is_external();
  set_clock_source(prev_external_clock);
  bool prev_external_clock_high = false;
  timer_reset();
  uint16_t prev_period = 0;
  uint32_t clock_delay = get_delay();
  uint64_t cycles_since_shift = 0;
  bool double_shift = false;
  while (1) {
    bool external_clock = clock_source_is_external();

    // Handle change to the clock source
    if (external_clock != prev_external_clock) {
      set_clock_source(external_clock);
      prev_external_clock = external_clock;
    }

    bool shift = get_shift_button();

    double_shift = shift && (double_shift || (cycles_since_shift > 0 && cycles_since_shift < SHIFT_DOUBLE_TAP_WINDOW));

    bool tick_this_frame;
    uint16_t timer_value = timer_read();
    clock_delay = get_delay();
    if (external_clock) {
      // TODO use time between last two ticks
      bool external_clock_high = get_clock_in();
      tick_this_frame = external_clock_high && !prev_external_clock_high;
      prev_external_clock_high = external_clock_high;
    } else {
      tick_this_frame = timer_value > clock_delay;
    }

    uint8_t num_steps = get_num_steps();
    if (prev_num_steps != num_steps) {
      prev_num_steps = num_steps;
      if (shift) {
        num_steps_display_countdown = NUM_STEPS_DISPLAY_DELAY;
      }
    }

    // Process buttons
    channels[0].pressed_now = !(PIND & BIT(5));
    channels[1].pressed_now = !(PIND & BIT(6));
    channels[2].pressed_now = !(PIND & BIT(7));
    channels[3].pressed_now = !(PINB & BIT(0));
    for (int i = 0; i < NUM_CHANNELS; i++) {
      channel_t *ch = &channels[i];
      if (ch->pressed_now) {
        uint8_t count_to_update = count;
        if (timer_value > (3 * prev_period) / 4) {
          count_to_update = (count_to_update + 1) % num_steps;
        }
        if (double_shift) {
          ch->sequence[count_to_update] = false;
        } else if (shift) {
          if (!ch->pressed_prev) {
            switch (i) {
              case 0:
                // Reset the sequence
                count = 0;
                timer_reset();
                break;
              case 1:
                reverse = !reverse;
                break;
              case 2:
                break;
              case 3:
                break;
            }
          }
        } else if (ch->allow_fill || !ch->pressed_prev) {
          ch->sequence[count_to_update] = true;
        }
      }
      uint32_t gate_threshold = ((uint32_t)ADC_read(ch->adc_index) * prev_period) / 4096;
      if (gate_threshold < MIN_GATE) {
        // make sure there's always a pulse
        gate_threshold = MIN_GATE;
      }
      if (ch->sequence[count] && (timer_value < gate_threshold)) {
        PORTB |= BIT(ch->out_portb_index);
      } else {
        PORTB &= ~BIT(ch->out_portb_index);
      }
      ch->pressed_prev = ch->pressed_now;
    }

    if (tick_this_frame) {
      if (reverse) {
        if (count == 0) {
          count = num_steps;
        }
        count--;
      } else {
        count++;
        if (count >= num_steps) {
          count = 0;
        }
      }
      timer_reset();
      prev_period = timer_value;
      set_clock_out(true);
    } else {
      set_clock_out(timer_value < (prev_period / 8));
    }

    if (num_steps_display_countdown > 0) {
      num_steps_display_countdown--;
      set_count_leds(num_steps);
    } else {
      set_count_leds(count);
    }

    if (shift) {
      cycles_since_shift = 0;
    } else {
      cycles_since_shift++;
    }

  }
  return 0;
}
