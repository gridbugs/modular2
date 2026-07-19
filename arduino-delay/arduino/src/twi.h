#pragma once

#include <stdbool.h>
#include <stdint.h>

int twi_transmit_start(void);
int twi_transmit_address(uint8_t address, bool write);
void twi_transmit_data_start(uint8_t data);
int twi_transmit_data_end(void);
int twi_transmit_data(uint8_t data);
void twi_transmit_stop(void);
