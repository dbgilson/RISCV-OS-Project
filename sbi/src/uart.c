#include <uart.h>
#include <ring_buff.h>

//Function to initialize the UART
void uart_init(void) {
    volatile char *uart = (char *)0x10000000;

    uart[3] = 3; // Word length select bits set to 11
    uart[2] = 1; //Enable FIFO
	uart[1] = 1; //Enable interrupts
}

//Function to get a char if there is a UART recieving pending flag
char uart_get(void) {
    volatile char *uart = (char *)0x10000000;
    if (uart[5] & 1){
      return uart[0];
    }
    return 255;
}

//Function to transmit a char on the UART bus
void uart_put(char c) {
    volatile char *uart = (char *)0x10000000;
    while(!(uart[5] & (1 << 6))); //If 0, transmitter is not empty
    uart[0] = c; //If transmitter is empty
}

//Now we are using a ring buffer with our UART communication
//from the OS.
void uart_handle_irq(void){
	char c;
    while((c = uart_get()) != 0xff){
        ring_put(c);
    }
}

