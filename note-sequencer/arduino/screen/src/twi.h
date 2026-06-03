#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>

int twi_transmit_start(void);
int twi_transmit_address(uint8_t address, bool write);
void twi_transmit_data_start(uint8_t data);
int twi_transmit_data_end(void);
int twi_transmit_data(uint8_t data);
void twi_transmit_stop(void);
int twi_send_bytes(uint8_t address, uint8_t *bytes, int nbytes);

// Enter "slave receive" mode on the given address. The TWI interrupt will fire
// when a packet is received. The ISR is expected to read TWSR (comparing it
// with the TWI_SR_STATUS_* values to determine the packet type) and possibly
// TWDR (if TWSR == TWI_SR_STATUS_DATA) directly, and to ack the interrupt by
// calling `twi_interrupt_ack`.
void twi_sr(uint8_t address);
void twi_interrupt_ack(void);

#define TWI_SR_STATUS_SLAW 0x60
#define TWI_SR_STATUS_DATA 0x80
#define TWI_SR_STATUS_STOP 0xA0
