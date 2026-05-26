#include "rtos_port.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern void ttak_worker_run_cooperative_bridge(ttak_worker_t *worker, uint64_t budget_ns);

#if defined(NATIVE_RUN) || !defined(TTAK_TARGET_ESP32)

void rtos_uart_console_init(rtos_uart_t *uart,
                            const char *label,
                            unsigned int baud,
                            void *hw) {
    (void)hw;
    uart->label = label;
    uart->baud = baud;
    uart->hw = NULL;
}

void rtos_uart_write(const rtos_uart_t *uart, const char *buf, size_t len) {
    (void)uart;
    if (!buf) return;
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
}

void rtos_uart_write_line(const rtos_uart_t *uart, const char *line) {
    if (!uart || !uart->label) return;
    printf("[%s] %s\n", uart->label, line ? line : "");
    fflush(stdout);
}

void rtos_uart_printf(const rtos_uart_t *uart, const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    rtos_uart_write_line(uart, buffer);
}

void rtos_port_scheduler_init(ttak_thread_pool_t *pool,
                              size_t worker_index,
                              uint32_t systick_hz) {
    (void)pool;
    (void)worker_index;
    (void)systick_hz;
}

void rtos_port_request_context_switch(void) {}
void rtos_port_tick_isr(void) {}
void rtos_port_pendsv_isr(void) {}

#else /* TTAK_TARGET_ESP32 */

#include <hal/esp32_wroom32.h>

static ttak_thread_pool_t *g_sched_pool = NULL;
static ttak_worker_t *g_sched_worker = NULL;
static uint32_t g_systick_hz = 1000U;
static esp32_hal_uart_t g_uart_stub;

void rtos_uart_console_init(rtos_uart_t *uart,
                            const char *label,
                            unsigned int baud,
                            void *hw) {
    uart->label = label;
    uart->baud = baud;
    uart->hw = hw ? hw : &g_uart_stub;
    esp32_hal_uart_init(uart->hw, baud);
}

void rtos_uart_write(const rtos_uart_t *uart, const char *buf, size_t len) {
    if (!buf || len == 0) return;
    esp32_hal_uart_write(uart->hw, buf, len);
}

void rtos_uart_write_line(const rtos_uart_t *uart, const char *line) {
    if (!line) return;
    esp32_hal_uart_write(uart->hw, line, strlen(line));
    const char newline[] = "\r\n";
    esp32_hal_uart_write(uart->hw, newline, sizeof(newline) - 1U);
}

void rtos_uart_printf(const rtos_uart_t *uart, const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    rtos_uart_write_line(uart, buffer);
}

void rtos_port_scheduler_init(ttak_thread_pool_t *pool,
                              size_t worker_index,
                              uint32_t systick_hz) {
    g_sched_pool = pool;
    g_sched_worker = (pool && worker_index < pool->num_threads) ? pool->workers[worker_index] : NULL;
    if (systick_hz > 0U) {
        g_systick_hz = systick_hz;
    }
    esp32_hal_tick_timer_start(g_systick_hz);
}

void rtos_port_request_context_switch(void) {
    esp32_hal_scheduler_pend();
}

void rtos_port_tick_isr(void) {
    if (g_sched_worker) {
        rtos_port_request_context_switch();
    }
}

void rtos_port_pendsv_isr(void) {
    if (!g_sched_worker) return;
    esp32_hal_isr_prologue();
    ttak_worker_run_cooperative_bridge(g_sched_worker, 0);
    esp32_hal_isr_epilogue();
}

#endif /* target selection */
