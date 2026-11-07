#pragma once

#include <stdint.h>

#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
#define NOP() __asm__ __volatile__("nop")
#define BIT(n) (((uint32_t)1) << n)
#define MASK(n) (BIT(n) - 1)
#define MIN_NAIVE(a, b) ((a) < (b) ? (a) : (b))
#define MAX_NAIVE(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP_NAIVE(min, value, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

__attribute__((noreturn)) static inline void loop_forever(void) {
  for (;;)
    ;
  __builtin_unreachable();
}

void delay_ms(uint16_t ms);
