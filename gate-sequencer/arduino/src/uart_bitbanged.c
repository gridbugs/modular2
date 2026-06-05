#include <avr/io.h>
#include <stdio.h>
#include "util.h"

// Bit duration measured in loop iterations (hand-tuned with oscilloscope)
#define BAUD_300_BIT_DELAY 2222

// Send a character over USART0.
static int USART0_bitbanged_tx(char data, struct __file* _f) {
    // Start bit
    PORTD &= ~BIT(1);
    for (long int i = BAUD_300_BIT_DELAY; i; i--);
    // Data bits
    for (int i = 0; i < 8; i++) {
        if (data & BIT(i)) {
            // Send a 1
            PORTD |= BIT(1);
        } else {
            // Send a 0
            PORTD &= ~BIT(1);
        }
        for (long int i = BAUD_300_BIT_DELAY; i; i--);
    }
    // Stop bit
    PORTD |= BIT(1);
    for (long int i = BAUD_300_BIT_DELAY; i; i--);
    return 0;
}

// Create a stream associated with transmitting data over USART0 (this will be
// used for stdout so we can print to a terminal with printf).
static FILE uartout_bitbanged = FDEV_SETUP_STREAM(USART0_bitbanged_tx, NULL, _FDEV_SETUP_WRITE);

void USART0_bitbanged_init(void) {
    // Set IO port pin corresponding to TX to be an output
    DDRD |= BIT(1);
    // Set the pin high as UART is high by default
    PORTD |= BIT(1);
    // Register stdout handler
    stdout = &uartout_bitbanged;
}
