#include "os/saramos_owner.h"
#include <string.h>

bool saramos_owner_init(saramos_owner_t *so, const char *name)
{
    (void)name; /* reserved for future tracing */

    if (!so)
        return false;

    memset(so, 0, sizeof(*so));

    so->owner = ttak_owner_create(TTAK_OWNER_SAFE_DEFAULT);
    return (so->owner != NULL);
}

void saramos_owner_destroy(saramos_owner_t *so)
{
    if (!so || !so->owner)
        return;

    ttak_owner_destroy(so->owner);
    so->owner = NULL;
}

bool saramos_owner_register_resource(saramos_owner_t *so, const char *name, void *data)
{
    if (!so || !so->owner)
        return false;

    return ttak_owner_register_resource(so->owner, name, data);
}

bool saramos_owner_register_func(saramos_owner_t *so, const char *name, ttak_owner_func_t func)
{
    if (!so || !so->owner)
        return false;

    return ttak_owner_register_func(so->owner, name, func);
}

bool saramos_owner_execute(saramos_owner_t *so, const char *func_name, const char *resource_name, void *args)
{
    if (!so || !so->owner)
        return false;

    return ttak_owner_execute(so->owner, func_name, resource_name, args);
}
