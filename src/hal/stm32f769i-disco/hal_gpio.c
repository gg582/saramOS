#include <hal/hal_gpio.h>

void hal_gpio_init_af(uint32_t port_base, uint8_t pin, uint8_t af, uint8_t speed, uint8_t pupd)
{
    volatile uint32_t *moder = gpio_reg(port_base, GPIO_MODER);
    volatile uint32_t *ospeedr = gpio_reg(port_base, GPIO_OSPEEDR);
    volatile uint32_t *pupdr = gpio_reg(port_base, GPIO_PUPDR);
    volatile uint32_t *afr = gpio_reg(port_base, (pin < 8) ? GPIO_AFRL : GPIO_AFRH);

    uint32_t mode_mask = 3U << (pin * 2);
    uint32_t mode_val = (uint32_t)GPIO_MODE_AF << (pin * 2);

    uint32_t speed_mask = 3U << (pin * 2);
    uint32_t speed_val = (uint32_t)speed << (pin * 2);

    uint32_t pupd_mask = 3U << (pin * 2);
    uint32_t pupd_val = (uint32_t)pupd << (pin * 2);

    uint32_t af_shift = (uint32_t)((pin & 7U) * 4U);
    uint32_t af_mask = 0xFU << af_shift;
    uint32_t af_val = ((uint32_t)af & 0xFU) << af_shift;

    *moder = (*moder & ~mode_mask) | mode_val;
    *ospeedr = (*ospeedr & ~speed_mask) | speed_val;
    *pupdr = (*pupdr & ~pupd_mask) | pupd_val;
    *afr = (*afr & ~af_mask) | af_val;
}

void hal_gpio_init_output(uint32_t port_base, uint8_t pin, uint8_t speed)
{
    volatile uint32_t *moder = gpio_reg(port_base, GPIO_MODER);
    volatile uint32_t *ospeedr = gpio_reg(port_base, GPIO_OSPEEDR);

    uint32_t mode_mask = 3U << (pin * 2);
    uint32_t mode_val = (uint32_t)GPIO_MODE_OUTPUT << (pin * 2);

    uint32_t speed_mask = 3U << (pin * 2);
    uint32_t speed_val = (uint32_t)speed << (pin * 2);

    *moder = (*moder & ~mode_mask) | mode_val;
    *ospeedr = (*ospeedr & ~speed_mask) | speed_val;
}

void hal_gpio_init_input(uint32_t port_base, uint8_t pin, uint8_t pupd)
{
    volatile uint32_t *moder = gpio_reg(port_base, GPIO_MODER);
    volatile uint32_t *pupdr = gpio_reg(port_base, GPIO_PUPDR);

    uint32_t mode_mask = 3U << (pin * 2);
    uint32_t mode_val = (uint32_t)GPIO_MODE_INPUT << (pin * 2);

    uint32_t pupd_mask = 3U << (pin * 2);
    uint32_t pupd_val = (uint32_t)pupd << (pin * 2);

    /* If the pin is already configured exactly as requested, leave it alone.
     * Re-writing the pull register can glitch a card-detect switch.
     */
    if (((*moder & mode_mask) == mode_val) &&
        ((*pupdr & pupd_mask) == pupd_val))
        return;

    *moder = (*moder & ~mode_mask) | mode_val;
    *pupdr = (*pupdr & ~pupd_mask) | pupd_val;
}

void hal_gpio_init_pullup(uint32_t port_base, uint8_t pin)
{
    hal_gpio_init_input(port_base, pin, GPIO_PUPD_UP);
}

void hal_gpio_init_pulldown(uint32_t port_base, uint8_t pin)
{
    hal_gpio_init_input(port_base, pin, GPIO_PUPD_DOWN);
}

void hal_gpio_write(uint32_t port_base, uint8_t pin, uint8_t val)
{
    volatile uint32_t *bsrr = gpio_reg(port_base, GPIO_BSRR);
    if (val) {
        *bsrr = (1U << pin);
    } else {
        *bsrr = (1U << (pin + 16U));
    }
}

int hal_gpio_read(uint32_t port_base, uint8_t pin)
{
    volatile uint32_t *idr = gpio_reg(port_base, GPIO_IDR);
    return (int)((*idr >> pin) & 1U);
}
