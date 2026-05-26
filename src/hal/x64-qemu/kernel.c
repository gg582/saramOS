#include "serial.h"

void kmain() {
    serial_init();
    serial_puts("\r\n--- saramOS Bare-metal x86_64 ---\r\n");
    serial_puts("Booted successfully into Long Mode.\r\n");
    serial_puts("Serial HAL is active at COM1 (0x3f8).\r\n");
    serial_puts("System state: Bare-metal, No POSIX dependencies.\r\n");
    serial_puts("\r\nEntering idle loop...\r\n");

    while (1) {
        asm volatile ("hlt");
    }
}
