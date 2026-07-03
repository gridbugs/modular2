#pragma once

#include <stdint.h>

#define COMPILER_BARRIER() __asm__ __volatile__ ("" ::: "memory")
#define BIT(n) (((uint32_t)1) << n)
#define MASK(n) (BIT(n) - 1)

void delay_ms(uint16_t ms);
