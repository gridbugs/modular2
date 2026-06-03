#pragma once

#include <stdint.h>

/* This uses the 16-bit timer in "normal mode" where it continuously counts
 * from 0 to 0xFFFF, wrapping around to 0. It runs at clkIO/1024. The expected
 * use-case is to reset the clock and then poll the `timer_read` function to
 * monitor the passage of time. */

void timer1_init(void);
void timer1_reset_and_start(void);
void timer1_stop(void);
uint16_t timer1_read(void);
void timer1_set_output_compare_a(uint16_t value);
void timer1_set_output_compare_b(uint16_t value);
void timer2_init_pwm_port_d_bit_3(uint8_t duty);
