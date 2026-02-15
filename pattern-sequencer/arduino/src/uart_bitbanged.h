// Register a stdout handler that sends over UART0, implemented with a
// bit-banging driver rather than using the UART hardware. Some cheap arduino
// clones don't have working UART hardware, so bit-banging is necessary.
//
// Port details:
// baudrate: 300
// parity: none
// stopbits: 1
void USART0_bitbanged_init(void);
