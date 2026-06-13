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

#define PORTB_OUT_1_BIT BIT(5)
#define PORTD_OUT_2_BIT BIT(1)
#define PORTD_OUT_3_BIT BIT(0)

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

#define NUM_STEPS_CHANGE_DISPLAY_TIME 4000
#define DOUBLE_TAP_TIME 1000

typedef enum {
  OUT_PORT_B,
  OUT_PORT_D,
} out_port_t;

typedef struct {
  out_port_t out_port;
  uint8_t out_port_bit;
  uint8_t btn_portd_bit;
  bool sequence[MAX_NUM_STEPS];
  bool prev_pressed;
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
  return (PINC & PORTC_CLOCK_SELECT_BIT) != 0;
}

void set_clock_source(bool external) {
  // Update the data-direction bit for the clock pin
  if (external) {
    // Clock input pin
    DDRC &= ~PORTC_CLOCK_BIT;
    // Make sure the clock's pull-up resistor is not active.
    PORTC &= ~PORTC_CLOCK_BIT;
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

bool is_mode_run(void) {
  return (PIND & PORTD_MODE_SELECT_BIT) != 0;
}

int main(void) {

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  //USART0_init();
  timer_init();

  // Initialize analog pins 6 and 7
  ADC_init(0xC0);


  // Initialize digital IO ports:
  DDRD |= (PORTD_OUT_2_BIT | PORTD_OUT_3_BIT);
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

  DDRB |= (PORTB_COUNT_MASK | PORTB_OUT_1_BIT);
  DDRB &= ~PORTB_SHIFT_BIT;
  PORTB = PORTB_SHIFT_BIT;

  DDRC |= PORTC_OUTPUT_MASK;
  DDRC &= ~PORTC_CLOCK_SELECT_BIT;
  PORTC = PORTC_CLOCK_SELECT_BIT;

  channel_t channels[NUM_CHANNELS] = {
    [0] = {
      .out_port = OUT_PORT_B,
      .out_port_bit = PORTB_OUT_1_BIT,
      .btn_portd_bit = PORTD_BTN_1_BIT,
      .sequence = { 0 },
      .prev_pressed = false,
    },
    [1] = {
      .out_port = OUT_PORT_D,
      .out_port_bit = PORTD_OUT_2_BIT,
      .btn_portd_bit = PORTD_BTN_2_BIT,
      .sequence = { 0 },
      .prev_pressed = false,
    },
    [2] = {
      .out_port = OUT_PORT_D,
      .out_port_bit = PORTD_OUT_3_BIT,
      .btn_portd_bit = PORTD_BTN_3_BIT,
      .sequence = { 0 },
      .prev_pressed = false,
    },
  };

  uint8_t count = 0;
  uint8_t divider_count = 0;
  uint8_t prev_num_steps = get_num_steps();
  uint16_t num_steps_change_display_timeout = 0;
  uint16_t shift_timeout = 0;
  bool prev_external_clock = clock_source_is_external();
  set_clock_source(prev_external_clock);
  bool prev_external_clock_high = false;
  timer_reset();
  uint16_t prev_period = 0;
  uint32_t clock_delay = get_delay();
  bool prev_shift = false;

  while (1) {

    bool running = is_mode_run();

    if (shift_timeout > 0) {
      shift_timeout--;
    }

    uint8_t num_steps = get_num_steps();
    if (prev_num_steps != num_steps) {
      prev_num_steps = num_steps;
      num_steps_change_display_timeout = NUM_STEPS_CHANGE_DISPLAY_TIME;
    } else if (num_steps_change_display_timeout > 0) {
      num_steps_change_display_timeout--;
      set_count_leds(num_steps);
    }

    bool shift = get_shift_button();
    bool shift_this_frame = shift && !prev_shift;
    prev_shift = shift;
    if (shift_this_frame && running) {
      if (shift_timeout > 0) {
        count = num_steps;
        divider_count = 0xFF;
      }
      shift_timeout = DOUBLE_TAP_TIME;
    }

    bool external_clock = clock_source_is_external();

    // Handle change to the clock source
    if (external_clock != prev_external_clock) {
      set_clock_source(external_clock);
      prev_external_clock = external_clock;
    }

    uint16_t timer_value = timer_read();
    clock_delay = get_delay();
    bool tick_this_frame;
    if (external_clock) {
      bool external_clock_high = get_clock_in();
      tick_this_frame = external_clock_high && !prev_external_clock_high;
      prev_external_clock_high = external_clock_high;
    } else {
      tick_this_frame = (running && shift_this_frame) || (timer_value > clock_delay);
    }

    if (!running) {
      if (shift_this_frame) {
        count++;
        if (count >= num_steps) {
          count = 0;
        }
      }
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
      channel_t *channel = &channels[i];
      bool *step_value = &channel->sequence[count];
      bool btn_pressed = (PIND & channel->btn_portd_bit) == 0;
      if (btn_pressed && (shift || !channel->prev_pressed)) {
        if (running) {
          *step_value = !shift;
        } else {
          *step_value = !*step_value;
        }
      }
      channel->prev_pressed = btn_pressed;
      bool out_value;
      if (!running || (timer_value < (prev_period / 2))) {
        out_value = *step_value;
      } else {
        out_value = false;
      }
      switch (channel->out_port) {
        case OUT_PORT_B:
          if (out_value) {
            PORTB |= channel->out_port_bit;
          } else {
            PORTB &= ~channel->out_port_bit;
          }
          break;
        case OUT_PORT_D:
          if (out_value) {
            PORTD |= channel->out_port_bit;
          } else {
            PORTD &= ~channel->out_port_bit;
          }
          break;
      }
    }

    if (tick_this_frame) {
      uint8_t prev_divider_count = divider_count;
      divider_count++;
      if (running) {
        count++;
        if (count >= num_steps) {
          count = 0;
        }
      }
      prev_period = timer_value;
      if (!external_clock) {
        set_clock_out(true);
      }
      set_clock_divider((divider_count ^ prev_divider_count) >> get_clock_divider_base());
      timer_reset();
    } else {
      if (timer_value > (prev_period / 2)) {
        if (!external_clock) {
          set_clock_out(false);
        }
        set_clock_divider(0);
      }
    }
    if (num_steps_change_display_timeout == 0) {
      set_count_leds(count);
    }
  }
  return 0;
}
