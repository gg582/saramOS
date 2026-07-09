/*
 * uptime - show how long the system has been running.
 */

#include <ttak/timing/timing.h>
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_uptime(int argc, char *argv[])
{
    unsigned long ticks = (unsigned long)ttak_get_tick_count();
    unsigned long seconds = ticks / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    char buf[64];
    snprintf(buf, sizeof(buf),
             "up %lu days, %02lu:%02lu:%02lu\r\n",
             days, hours % 24, minutes % 60, seconds % 60);
    hal_uart_puts(buf);
    return 0;
}
