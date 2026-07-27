#pragma once

#include <stdint.h>

static inline void timer1_init(void) {
    TCCR1A = 0; // Normal mode
}

static inline void timer1_enable_interrupt_output_compare_a(void) {
    TIMSK1 |= BIT(OCIE1A);
}

static inline void timer1_set_output_compare_a(uint16_t value) {
  OCR1A = value;
}

static inline void timer1_set_reset_on_output_compare_a_match(void) {
  TCCR1A |= BIT(WGM10);
  TCCR1B |= BIT(WGM13);
}

static inline void timer1_reset(void) {
    TCNT1 = 0;
}

static inline void timer1_start(void) {
    TCCR1B |= BIT(CS12); // clk_io / 256
}

static inline void timer1_stop(void) {
    TCCR1B &= ~(BIT(CS12) | BIT(CS11) | BIT(CS10));
}

static inline uint16_t timer1_read(void) {
    return TCNT1;
}

void timer2_init_pwm_port_d_bit_3(uint8_t duty);
