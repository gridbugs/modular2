#pragma once

#include <stdio.h>
#include <stdint.h>

#define COMPILER_BARRIER() __asm__ __volatile__ ("" ::: "memory")
#define BIT(n) (((uint32_t)1) << n)
#define MASK(n) (BIT(n) - 1)

#define PANIC(...) do { \
  printf(__VA_ARGS__); \
  printf("\n\r"); \
  while(1); \
} while(0);

void delay_ms(uint16_t ms);
