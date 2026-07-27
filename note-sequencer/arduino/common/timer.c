#include <stdint.h>
#include <avr/io.h>
#include "util.h"

void timer2_init_pwm_port_d_bit_3(uint8_t duty) {
  DDRD |= BIT(3);
  TCCR2A = BIT(COM2B1) | BIT(WGM21) | BIT(WGM20);
  TCCR2B = BIT(CS22);
  OCR2B = duty;
}
