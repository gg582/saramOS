#ifndef SARAMOS_ARENA_H
#define SARAMOS_ARENA_H

#include <ttak/mem/arena_helper.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief saramOS arena wrapper around libttak generational arena.
 *
 * Manages a single active generation.  Rotating the arena starts a fresh
 * generation and retires the previous one through libttak epoch GC.
 */
typedef struct {
    ttak_arena_env_t env;
    ttak_arena_generation_t generation;
    uint32_t epoch_counter;
    bool generation_active;
} saramos_arena_t;

/**
 * @brief Initialize the system arena with conservative bare-metal defaults.
 *
 * @param arena Pointer to arena state (uninitialized).
 * @return true on success, false on failure.
 */
bool saramos_arena_init(saramos_arena_t *arena);

/**
 * @brief Tear down the arena and retire any active generation.
 */
void saramos_arena_destroy(saramos_arena_t *arena);

/**
 * @brief Allocate memory from the current active generation.
 *
 * If no generation is active, one is started automatically.
 *
 * @param arena Arena context.
 * @param bytes Number of bytes requested.
 * @return Pointer to zeroed memory inside the generation, or NULL.
 */
void *saramos_arena_alloc(saramos_arena_t *arena, size_t bytes);

/**
 * @brief Reset the current generation (discard all allocations in place).
 *
 * This is cheaper than rotate() because it does not run epoch reclamation.
 */
void saramos_arena_reset(saramos_arena_t *arena);

/**
 * @brief Rotate to a new generation.
 *
 * The old generation is retired and will be reclaimed by the epoch GC later.
 */
void saramos_arena_rotate(saramos_arena_t *arena);

/**
 * @brief Return remaining bytes in the current generation.
 */
size_t saramos_arena_remaining(const saramos_arena_t *arena);

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_ARENA_H */
