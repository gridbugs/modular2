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

static inline void timer1_set_output_compare_b(uint16_t value) {
  OCR1B = value;
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

static inline void timer2_init(void) {
    TCCR2A = 0; // Normal mode
}

static inline void timer2_reset(void) {
    TCNT2 = 0;
}

static inline void timer2_start(void) {
    TCCR2B |= BIT(CS22); // clk_io / 128
}

static inline uint8_t timer2_read(void) {
    return TCNT2;
}

static inline void timer2_set_output_compare_a(uint8_t value) {
  OCR2A = value;
}

static inline void timer2_enable_interrupt_output_compare_a(void) {
    TIMSK2 |= BIT(OCIE2A);
}

void timer2_init_pwm_port_d_bit_3(uint8_t duty);
