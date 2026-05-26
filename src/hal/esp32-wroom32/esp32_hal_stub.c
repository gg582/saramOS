#include <hal/esp32_wroom32.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(TTAK_TARGET_ESP32) && defined(__XTENSA__)

/* Minimal register definitions extracted from the ESP-IDF UART headers. */
#define DR_REG_UART_BASE 0x3ff40000U
#define DR_REG_RTCCNTL_BASE 0x3ff48000U
#define DR_REG_TIMERGROUP0_BASE 0x3ff5f000U
#define DR_REG_TIMERGROUP1_BASE 0x3ff60000U
#define REG_UART_BASE(i) (DR_REG_UART_BASE + (uint32_t)(i) * 0x10000U + ((i) > 1 ? 0xe000U : 0U))
#define UART_FIFO_REG(i) (REG_UART_BASE(i) + 0x0U)
#define UART_CLKDIV_REG(i) (REG_UART_BASE(i) + 0x14U)
#define UART_STATUS_REG(i) (REG_UART_BASE(i) + 0x1cU)
#define UART_TXFIFO_CNT_S 16
#define UART_TXFIFO_CNT 0xFFU
#define ESP32_HAL_UART_PORT 0U
#define ESP32_APB_CLK_FREQ 80000000U
#define ESP32_HAL_WDT_KEY 0x50d83aa1U
#define RTC_CNTL_WDTCONFIG0_REG (DR_REG_RTCCNTL_BASE + 0x8cU)
#define RTC_CNTL_WDTCONFIG1_REG (DR_REG_RTCCNTL_BASE + 0x90U)
#define RTC_CNTL_WDTCONFIG2_REG (DR_REG_RTCCNTL_BASE + 0x94U)
#define RTC_CNTL_WDTCONFIG3_REG (DR_REG_RTCCNTL_BASE + 0x98U)
#define RTC_CNTL_WDTCONFIG4_REG (DR_REG_RTCCNTL_BASE + 0x9cU)
#define RTC_CNTL_WDTWPROTECT_REG (DR_REG_RTCCNTL_BASE + 0xa4U)
#define REG_TIMG_BASE(i) (DR_REG_TIMERGROUP0_BASE + (uint32_t)(i) * 0x1000U)
#define TIMG_WDTCONFIG0_REG(i) (REG_TIMG_BASE(i) + 0x0048U)
#define TIMG_WDTCONFIG1_REG(i) (REG_TIMG_BASE(i) + 0x004cU)
#define TIMG_WDTCONFIG2_REG(i) (REG_TIMG_BASE(i) + 0x0050U)
#define TIMG_WDTCONFIG3_REG(i) (REG_TIMG_BASE(i) + 0x0054U)
#define TIMG_WDTCONFIG4_REG(i) (REG_TIMG_BASE(i) + 0x0058U)
#define TIMG_WDTCONFIG5_REG(i) (REG_TIMG_BASE(i) + 0x005cU)
#define TIMG_WDTWPROTECT_REG(i) (REG_TIMG_BASE(i) + 0x0064U)

static inline void esp32_hal_reg_write(uint32_t addr, uint32_t value) {
    *((volatile uint32_t *)addr) = value;
}

static inline uint32_t esp32_hal_reg_read(uint32_t addr) {
    return *((volatile uint32_t *)addr);
}

static void esp32_hal_disable_rtc_wdt(void) {
    esp32_hal_reg_write(RTC_CNTL_WDTWPROTECT_REG, ESP32_HAL_WDT_KEY);
    esp32_hal_reg_write(RTC_CNTL_WDTCONFIG0_REG, 0);
    esp32_hal_reg_write(RTC_CNTL_WDTCONFIG1_REG, 0);
    esp32_hal_reg_write(RTC_CNTL_WDTCONFIG2_REG, 0);
    esp32_hal_reg_write(RTC_CNTL_WDTCONFIG3_REG, 0);
    esp32_hal_reg_write(RTC_CNTL_WDTCONFIG4_REG, 0);
    esp32_hal_reg_write(RTC_CNTL_WDTWPROTECT_REG, 0);
}

static void esp32_hal_disable_timg_wdt(int idx) {
    esp32_hal_reg_write(TIMG_WDTWPROTECT_REG(idx), ESP32_HAL_WDT_KEY);
    esp32_hal_reg_write(TIMG_WDTCONFIG0_REG(idx), 0);
    esp32_hal_reg_write(TIMG_WDTCONFIG1_REG(idx), 0);
    esp32_hal_reg_write(TIMG_WDTCONFIG2_REG(idx), 0);
    esp32_hal_reg_write(TIMG_WDTCONFIG3_REG(idx), 0);
    esp32_hal_reg_write(TIMG_WDTCONFIG4_REG(idx), 0);
    esp32_hal_reg_write(TIMG_WDTCONFIG5_REG(idx), 0);
    esp32_hal_reg_write(TIMG_WDTWPROTECT_REG(idx), 0);
}

static void esp32_hal_disable_watchdogs(void) {
    esp32_hal_disable_rtc_wdt();
    esp32_hal_disable_timg_wdt(0);
    esp32_hal_disable_timg_wdt(1);
}

static void esp32_hal_uart_program_baud(unsigned int baud) {
    if (baud == 0U) {
        baud = 115200U;
    }
    uint32_t divider = ((ESP32_APB_CLK_FREQ << 4) / baud) & 0xFFFFFU;
    uint32_t clkdiv = esp32_hal_reg_read(UART_CLKDIV_REG(ESP32_HAL_UART_PORT));
    clkdiv &= ~0xFFFFFU;
    clkdiv |= divider;
    esp32_hal_reg_write(UART_CLKDIV_REG(ESP32_HAL_UART_PORT), clkdiv);
}

static void esp32_hal_uart_wait_room(void) {
    while (((esp32_hal_reg_read(UART_STATUS_REG(ESP32_HAL_UART_PORT)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) >= 126U) {
        /* busy wait */
    }
}

static void esp32_hal_bootstrap(void) {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    esp32_hal_disable_watchdogs();
    initialized = true;
}

void esp32_hal_early_init(void) {
    esp32_hal_bootstrap();
}

void esp32_hal_uart_init(esp32_hal_uart_t *dev, unsigned int baud) {
    if (!dev) {
        return;
    }
    esp32_hal_bootstrap();
    dev->baud = baud ? baud : 115200U;
    esp32_hal_uart_program_baud(dev->baud);
}

void esp32_hal_uart_write(esp32_hal_uart_t *dev, const char *buf, size_t len) {
    (void)dev;
    if (!buf || len == 0) {
        return;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)buf[i];
        if (ch == '\n') {
            esp32_hal_uart_wait_room();
            esp32_hal_reg_write(UART_FIFO_REG(ESP32_HAL_UART_PORT), '\r');
        }
        esp32_hal_uart_wait_room();
        esp32_hal_reg_write(UART_FIFO_REG(ESP32_HAL_UART_PORT), ch);
    }
}

void esp32_hal_tick_timer_start(uint32_t hz) {
    (void)hz;
    /* TODO: hook an esp_timer interrupt to drive libTTAK's cooperative worker. */
}

void esp32_hal_scheduler_pend(void) {}
void esp32_hal_isr_prologue(void) {}
void esp32_hal_isr_epilogue(void) {}

#else /* host stub */

#include <stdio.h>

void esp32_hal_early_init(void) {}

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
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
}

void esp32_hal_tick_timer_start(uint32_t hz) {
    (void)hz;
}

void esp32_hal_scheduler_pend(void) {}
void esp32_hal_isr_prologue(void) {}
void esp32_hal_isr_epilogue(void) {}

#endif /* target selection */
