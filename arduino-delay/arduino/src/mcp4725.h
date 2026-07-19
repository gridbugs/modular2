#include <stdint.h>

#define MCP4725_ADDR0 0x60
#define MCP4725_ADDR1 0x61

#define MCP4725_COMMAND_UPDATE 64

// Set the value of the DAC with I2C address 0x60
int dac0_set_value(uint16_t value);

// Set the value of the DAC with I2C address 0x61 (A0 pin is pulled high)
int dac1_set_value(uint16_t value);
