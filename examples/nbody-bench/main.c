#define _POSIX_C_SOURCE 200809L
#include "nbody_bench.h"

#ifdef NATIVE_RUN
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#else
#include <stdint.h>
#endif

#ifndef NBODY_DEMO_BODIES
#define NBODY_DEMO_BODIES 64U
#endif

#ifdef NATIVE_RUN
static Body g_initial_bodies[NBODY_MAX_BODIES];
static pthread_t g_threads[NBODY_MAX_TASKS];
static NBodyTaskConfig g_tasks[NBODY_MAX_TASKS];
static uint32_t g_task_count;

static void *task_shim(void *arg) {
    NBodyTaskConfig *cfg = (NBodyTaskConfig *)arg;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ready_cycle = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    nbody_mark_task_ready(cfg->task_id, ready_cycle);
    nbody_task_entry(cfg);
    return NULL;
}

static void build_initial_bodies(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        double mass = 1.0 + (double)(i % 5);
        double angle = (double)i * 0.1;
        g_initial_bodies[i].mass = mass;
        g_initial_bodies[i].pos[0] = cos(angle) * 1000.0;
        g_initial_bodies[i].pos[1] = sin(angle) * 1000.0;
        g_initial_bodies[i].pos[2] = (double)(i % 7) * 100.0;
        g_initial_bodies[i].vel[0] = -sin(angle) * 0.1;
        g_initial_bodies[i].vel[1] = cos(angle) * 0.1;
        g_initial_bodies[i].vel[2] = 0.0;
    }
}

static void host_yield(void) {
    sched_yield();
}

int main(void) {
    const uint32_t body_count = NBODY_DEMO_BODIES;
    build_initial_bodies(body_count);

    NBodySimConfig cfg = {
        .body_count = body_count,
        .task_count = 4,
        .steps = 128,
        .delta_t = 0.01,
        .softening = 1e-3,
        .grav_const = 6.67430e-11,
        .cpu_hz = 1.0e9,
    };

    nbody_bench_init(&cfg, g_initial_bodies, body_count);
    NBodyOSHooks hooks = { .yield = host_yield };
    nbody_bench_bind_hooks(&hooks);

    const NBodyTaskConfig *table = nbody_bench_get_task_table(&g_task_count);
    memcpy(g_tasks, table, g_task_count * sizeof(NBodyTaskConfig));

    nbody_bench_start();

    for (uint32_t i = 0; i < g_task_count; ++i) {
        (void)pthread_create(&g_threads[i], NULL, task_shim, &g_tasks[i]);
    }

    for (uint32_t i = 0; i < g_task_count; ++i) {
        pthread_join(g_threads[i], NULL);
    }

    nbody_bench_wait_for_completion();
    nbody_bench_report();
    return 0;
}
#else
int main(void) {
    /* Placeholder so the cross build succeeds; integrate with your RTOS entry. */
    return 0;
}
#endif
