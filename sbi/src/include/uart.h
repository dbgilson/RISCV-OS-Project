#pragma once

// Initialize the UART system.
void uart_init(void);

// Transmit a single character
void uart_put(char c);

// Receive a single character
char uart_get(void);

// Handles plic interrupt for UART
void uart_handle_irq(void);
