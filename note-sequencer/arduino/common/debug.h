#pragma once

#include <stdio.h>
#include "util.h"

#define PANIC(...) do { \
  printf(__VA_ARGS__); \
  printf("\n\r"); \
  loop_forever(); \
} while(0);

#define DEBUG_PRINTING
#ifdef DEBUG_PRINTING
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...)
#endif
