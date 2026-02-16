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

uint32_t get_delay(void) {
    uint32_t tempo_adc = (uint32_t)ADC_read(6);
    return (4096 + 128) - tempo_adc;
}

uint8_t get_num_steps(void) {
    uint16_t num_steps_adc = ((uint16_t)ADC_read(7));
    return (uint8_t)(num_steps_adc >> 7) + 1;
}

bool get_clear_button(void) {
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

#define NUM_STEPS_DISPLAY_DELAY 2000

int main(void) {

    // Wait for debouncing capacitors to charge.
    for (long int i = 0; i < 1000000; i++);

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

    uint32_t cycles_since_last_tick = 0;
    uint8_t prev_num_steps = get_num_steps();
    uint32_t num_steps_display_countdown = 0;
    bool prev_external_clock = clock_source_is_external();
    set_clock_source(prev_external_clock);
    bool prev_external_clock_high = false;
    while (1) {
        bool external_clock = clock_source_is_external();

        // Handle change to the clock source
        if (external_clock != prev_external_clock) {
            set_clock_source(external_clock);
            prev_external_clock = external_clock;
        }

        uint32_t delay;
        bool tick_this_frame;
        if (external_clock) {
            // TODO use time between last two ticks
            delay = get_delay();
            bool external_clock_high = get_clock_in();
            tick_this_frame = external_clock_high && !prev_external_clock_high;
            prev_external_clock_high = external_clock_high;
        } else {
            delay = get_delay();
            tick_this_frame = cycles_since_last_tick >= delay;
        }

        // Process buttons
        bool clear = get_clear_button();
        channels[0].pressed_now = !(PIND & BIT(5));
        channels[1].pressed_now = !(PIND & BIT(6));
        channels[2].pressed_now = !(PIND & BIT(7));
        channels[3].pressed_now = !(PINB & BIT(0));
        for (int i = 0; i < NUM_CHANNELS; i++) {
            channel_t *ch = &channels[i];
            if (ch->pressed_now) {
                if (clear) {
                    ch->sequence[count] = false;
                } else if (ch->allow_fill || !ch->pressed_prev) {
                    ch->sequence[count] = true;
                }
            }
            ch->pressed_prev = ch->pressed_now;
            uint32_t threshold = ((uint32_t)ADC_read(ch->adc_index) * delay) / 4096;
            threshold = threshold == 0 ? 1 : threshold;
            if (ch->sequence[count] && (cycles_since_last_tick < threshold)) {
                PORTB |= BIT(ch->out_portb_index);
            } else {
                PORTB &= ~BIT(ch->out_portb_index);
            }
        }

        uint8_t num_steps = get_num_steps();
        if (prev_num_steps != num_steps) {
            prev_num_steps = num_steps;
            num_steps_display_countdown = NUM_STEPS_DISPLAY_DELAY;
        }
        if (tick_this_frame) {
            count += 1;
            if (count >= num_steps) {
                count = 0;
            }
            cycles_since_last_tick = 0;
        } else {
            cycles_since_last_tick += 1;
        }
        set_clock_out(cycles_since_last_tick < (delay / 2));
        if (num_steps_display_countdown > 0) {
            num_steps_display_countdown--;
            set_count_leds(num_steps);
        } else {
            set_count_leds(count);
        }
    }

    return 0;
}
