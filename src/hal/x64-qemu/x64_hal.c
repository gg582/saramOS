#include <hal/esp32_wroom32.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* Minimal HAL for x64-qemu using POSIX standard I/O */

void esp32_hal_early_init(void) {
    /* No special hardware initialization needed for POSIX runner */
}

void esp32_hal_uart_init(esp32_hal_uart_t *dev, unsigned int baud) {
    if (dev) {
        dev->baud = baud;
    }
}

void esp32_hal_uart_write(esp32_hal_uart_t *dev, const char *buf, size_t len) {
    (void)dev;
    if (!buf || len == 0) {
        return;
    }
    ssize_t written = write(STDOUT_FILENO, buf, len);
    (void)written;
    fsync(STDOUT_FILENO);
}

void esp32_hal_tick_timer_start(uint32_t hz) {
    (void)hz;
    /* On POSIX, we rely on the host's thread scheduling and timer mechanisms.
     * libTTAK's native profile handles its own timing. */
}

void esp32_hal_scheduler_pend(void) {}
void esp32_hal_isr_prologue(void) {}
void esp32_hal_isr_epilogue(void) {}
