#include <hal/stm32f769i-disc1.h>

void hal_system_init(void)
{
    /* Enable I-Cache and D-Cache for Cortex-M7 */
    __asm volatile (
        "dsb\n\t"
        "isb\n\t"
    );

    /* Set FLASH latency for 16 MHz (0 wait states is enough) */
    FLASH_ACR &= ~0xFU;
    FLASH_ACR |= 0U; /* 0 WS */

    /* Keep HSI as system clock (default after reset) */
    /* No PLL setup needed for minimal bring-up */
}
