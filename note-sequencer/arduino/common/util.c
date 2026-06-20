#include <stdint.h>

#define DELAY_MS_INNER_ITERATIONS_PER_MS 600

void delay_ms(uint16_t ms) {
  for (uint16_t i = 0; i < ms; i++) {
    for (uint16_t j = 0; j < DELAY_MS_INNER_ITERATIONS_PER_MS; j++) {
      __asm__ __volatile__("nop");
    }
  }
}
