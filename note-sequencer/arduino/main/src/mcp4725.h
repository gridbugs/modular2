#include <stdint.h>

// Set the value of the DAC with I2C address 0x60
int dac0_set_value(uint16_t value);

// Set the value of the DAC with I2C address 0x61 (A0 pin is pulled high)
int dac1_set_value(uint16_t value);
