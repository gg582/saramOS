#ifndef SARAMOS_OWNER_H
#define SARAMOS_OWNER_H

#include <ttak/mem/owner.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief saramOS resource owner wrapper around libttak ttak_owner_t.
 *
 * Each subsystem (UART, scheduler, etc.) can have its own owner.
 * Resources registered to an owner are isolated and cleaned up automatically
 * when the owner is destroyed.
 */
typedef struct {
    ttak_owner_t *owner;
} saramos_owner_t;

/**
 * @brief Create a new owner context.
 *
 * @param so   Pointer to wrapper state.
 * @param name Subsystem name (used for diagnostics; not copied).
 * @return true on success, false on failure.
 */
bool saramos_owner_init(saramos_owner_t *so, const char *name);

/**
 * @brief Destroy the owner and release all registered resources.
 */
void saramos_owner_destroy(saramos_owner_t *so);

/**
 * @brief Register a raw resource pointer under a unique name.
 *
 * @param so   Owner context.
 * @param name Unique resource name.
 * @param data Opaque resource pointer.
 * @return true on success.
 */
bool saramos_owner_register_resource(saramos_owner_t *so, const char *name, void *data);

/**
 * @brief Register a function that can be executed inside the owner's context.
 *
 * @param so   Owner context.
 * @param name Unique function name.
 * @param func Function pointer.
 * @return true on success.
 */
bool saramos_owner_register_func(saramos_owner_t *so, const char *name, ttak_owner_func_t func);

/**
 * @brief Execute a previously registered function, optionally passing a registered resource as ctx.
 *
 * @param so            Owner context.
 * @param func_name     Name of the function to execute.
 * @param resource_name Name of a registered resource to use as ctx (may be NULL).
 * @param args          Extra arguments forwarded to the function.
 * @return true if the function was found and executed.
 */
bool saramos_owner_execute(saramos_owner_t *so, const char *func_name, const char *resource_name, void *args);

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_OWNER_H */
