#include <stdint.h>
#include <avr/io.h>
#include "util.h"

void timer1_init(void) {
    // Normal mode
    TCCR1A = 0;

    // Enable interrupts
    TIMSK1 |= BIT(TOIE1) | BIT(OCIE1A) | BIT(OCIE1B);
}

void timer1_reset_and_start(void) {
    TCNT1 = 0;
    TCCR1B = BIT(CS11);
}

void timer1_stop(void) {
    TCCR1B = 0;
}

uint16_t timer1_read(void) {
    return TCNT1;
}

void timer1_set_output_compare_a(uint16_t value) {
  OCR1A = value;
}

void timer1_set_output_compare_b(uint16_t value) {
  OCR1B = value;
}

void timer2_init_pwm_port_d_bit_3(uint8_t duty) {
  DDRD |= BIT(3);
  TCCR2A = BIT(COM2B1) | BIT(WGM21) | BIT(WGM20);
  TCCR2B = BIT(CS22);
  OCR2B = duty;
}
