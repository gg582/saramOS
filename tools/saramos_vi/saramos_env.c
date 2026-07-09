/*
 * Minimal environment stub for the saramOS vi port.
 */

#include <string.h>
#include "saramos_port.h"

char *saramos_getenv(const char *name)
{
    if (strcmp(name, "TERM") == 0)
        return "vt100";
    if (strcmp(name, "HOME") == 0)
        return "0:/";
    return NULL;
}
