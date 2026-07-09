/*
 * date - print the system date and time.
 *
 * Since there is no RTC, this is derived from the tick counter.
 */

#include <ttak/timing/timing.h>
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_date(int argc, char *argv[])
{
    unsigned long ticks = (unsigned long)ttak_get_tick_count();
    unsigned long seconds = ticks / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    char buf[64];
    snprintf(buf, sizeof(buf),
             "%02lu:%02lu:%02lu (tick %lu)\r\n",
             hours % 24, minutes % 60, seconds % 60, ticks);
    hal_uart_puts(buf);
    return 0;
}
