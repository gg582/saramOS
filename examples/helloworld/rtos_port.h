#ifndef LIBTTAK_RTOS_PORT_H
#define LIBTTAK_RTOS_PORT_H

#include <hal/esp32_wroom32.h>

#include <stddef.h>
#include <stdint.h>

#include <ttak/thread/pool.h>
#include <ttak/thread/worker.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *label;
    unsigned int baud;
    void *hw; /* esp_hal_uart_t* when targeting ESP32 */
} rtos_uart_t;

void rtos_uart_console_init(rtos_uart_t *uart,
                            const char *label,
                            unsigned int baud,
                            void *hw);
void rtos_uart_write(const rtos_uart_t *uart, const char *buf, size_t len);
void rtos_uart_write_line(const rtos_uart_t *uart, const char *line);
void rtos_uart_printf(const rtos_uart_t *uart, const char *fmt, ...);

void rtos_port_scheduler_init(ttak_thread_pool_t *pool,
                              size_t worker_index,
                              uint32_t systick_hz);
void rtos_port_request_context_switch(void);
void rtos_port_tick_isr(void);
void rtos_port_pendsv_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBTTAK_RTOS_PORT_H */
