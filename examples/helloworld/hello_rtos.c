#include "rtos_port.h"
#include <stdio.h>

void main(void) {
    rtos_uart_t uart;
    rtos_uart_console_init(&uart, "uart0", 115200U, NULL);

    while (1) {
//        rtos_uart_write_line(&uart, "Hello from saramOS!");
        printf("Hello World");
        for (volatile int i = 0; i < 5000000; i++) { __asm__("nop"); }
    }
}
