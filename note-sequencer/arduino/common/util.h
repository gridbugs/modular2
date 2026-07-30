#pragma once

#include <stdio.h>
#include <stdint.h>

#define COMPILER_BARRIER() __asm__ __volatile__ ("" ::: "memory")
#define NOP() __asm__ __volatile__("nop")
#define BIT(n) (((uint32_t)1) << n)
#define MASK(n) (BIT(n) - 1)

__attribute__((noreturn))
static inline void loop_forever(void) {
  for (;;);
  __builtin_unreachable();
}

#define PANIC(...) do { \
  printf(__VA_ARGS__); \
  printf("\n\r"); \
  loop_forever(); \
} while(0);

void delay_ms(uint16_t ms);
