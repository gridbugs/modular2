#include <stdint.h>
#include <avr/io.h>
#include "util.h"

void timer1_init(void) {
    // Normal mode, clocked at clkIO/1024
    TCCR1A = 0;
    TCCR1B = BIT(0) | BIT(2);
}

void timer1_reset(void) {
    TCNT1 = 0;
}

uint16_t timer1_read(void) {
    return TCNT1;
}
