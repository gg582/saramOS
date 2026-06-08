#include "os/saramos_arena.h"
#include <string.h>

bool saramos_arena_init(saramos_arena_t *arena)
{
    if (!arena)
        return false;

    memset(arena, 0, sizeof(*arena));

    ttak_arena_env_config_t config;
    ttak_arena_env_config_init(&config);

    /* Conservative bare-metal defaults */
    config.generation_bytes = 4096;
    config.chunk_bytes = 256;
    config.alloc_flags = TTAK_MEM_DEFAULT;
    config.lifetime_ticks = __TTAK_UNSAFE_MEM_FOREVER__;

    if (!ttak_arena_env_init(&arena->env, &config))
        return false;

    arena->epoch_counter = 1;
    arena->generation_active = false;
    return true;
}

void saramos_arena_destroy(saramos_arena_t *arena)
{
    if (!arena)
        return;

    if (arena->generation_active) {
        ttak_arena_generation_retire(&arena->env, &arena->generation);
        arena->generation_active = false;
    }

    ttak_arena_env_destroy(&arena->env);
    memset(arena, 0, sizeof(*arena));
}

void *saramos_arena_alloc(saramos_arena_t *arena, size_t bytes)
{
    if (!arena)
        return NULL;

    if (!arena->generation_active) {
        if (!ttak_arena_generation_begin(&arena->env, &arena->generation, arena->epoch_counter))
            return NULL;
        arena->generation_active = true;
    }

    return ttak_arena_generation_claim(&arena->env, &arena->generation, bytes);
}

void saramos_arena_reset(saramos_arena_t *arena)
{
    if (!arena || !arena->generation_active)
        return;

    ttak_arena_generation_reset(&arena->generation);
    arena->generation.used = 0;
}

void saramos_arena_rotate(saramos_arena_t *arena)
{
    if (!arena)
        return;

    if (arena->generation_active) {
        ttak_arena_generation_retire(&arena->env, &arena->generation);
        arena->generation_active = false;
    }

    arena->epoch_counter++;
    ttak_arena_env_rotate(&arena->env);
}

size_t saramos_arena_remaining(const saramos_arena_t *arena)
{
    if (!arena || !arena->generation_active)
        return 0;

    return ttak_arena_generation_remaining(&arena->generation);
}
