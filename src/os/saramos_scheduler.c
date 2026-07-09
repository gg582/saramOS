#include <os/saramos_scheduler.h>
#include <os/saramos_process.h>
#include <os/saramos_pipe.h>
#include <string.h>
#include <stdio.h>

volatile uint32_t saramos_tick_ms = 0;

static saramos_process_t *ready_head = NULL;
static saramos_process_t *idle_proc = NULL;

void saramos_sched_add_ready(saramos_process_t *p)
{
    if (!p || p->state != SARAMOS_PROC_READY)
        return;

    /* Avoid duplicates. */
    saramos_process_t *cur = ready_head;
    while (cur) {
        if (cur == p)
            return;
        cur = cur->next_ready;
    }

    p->next_ready = ready_head;
    ready_head = p;
}

void saramos_sched_remove_ready(saramos_process_t *p)
{
    if (!p)
        return;

    saramos_process_t **pp = &ready_head;
    while (*pp) {
        if (*pp == p) {
            *pp = p->next_ready;
            p->next_ready = NULL;
            return;
        }
        pp = &(*pp)->next_ready;
    }
}

void saramos_sched_block(saramos_process_t *p)
{
    saramos_proc_block(p);
}

void saramos_sched_unblock(saramos_process_t *p)
{
    saramos_proc_unblock(p);
}

static void rotate_ready(saramos_process_t *p)
{
    if (!p || !ready_head)
        return;

    saramos_sched_remove_ready(p);

    /* Move p to the tail after removal; otherwise a non-head tail node can
     * leave tail pointing at p->next_ready and create a self-loop. */
    saramos_process_t **tail = &ready_head;
    while (*tail)
        tail = &(*tail)->next_ready;

    p->next_ready = NULL;
    *tail = p;
}

uint8_t saramos_sched_weight_for_name(const char *name)
{
    if (!name)
        return SARAMOS_SCHED_WEIGHT_NORMAL;

    static const char *core_prefixes[] = { SARAMOS_SCHED_CORE_PREFIXES };
    for (size_t i = 0; i < sizeof(core_prefixes) / sizeof(core_prefixes[0]); i++) {
        const char *pre = core_prefixes[i];
        size_t plen = strlen(pre);
        if (strncmp(name, pre, plen) == 0)
            return SARAMOS_SCHED_WEIGHT_CORE;
    }
    return SARAMOS_SCHED_WEIGHT_NORMAL;
}

void saramos_sched_yield(void)
{
    if (saramos_current_proc) {
        saramos_current_proc->state = SARAMOS_PROC_READY;
    }
}

static int idle_process(saramos_process_t *p)
{
    (void)p;
    /* Idle body: the scheduler runs house-keeping after each step. */
    PROC_BEGIN(p);
    while (1) {
        PROC_YIELD(p);
    }
    PROC_END(p);
}

void saramos_sched_init(void)
{
    saramos_proc_init();
    ready_head = NULL;

    idle_proc = saramos_proc_spawn("idle", idle_process, 0, NULL, NULL, NULL, SARAMOS_SCHED_WEIGHT_NORMAL);
    if (idle_proc) {
        idle_proc->state = SARAMOS_PROC_READY;
        saramos_sched_add_ready(idle_proc);
    }
}

/* Weak hook for board-specific housekeeping (lwIP polling, background tasks). */
__attribute__((weak)) void saramos_sched_housekeeping(void)
{
}

void saramos_sched_run(void)
{
    while (1) {
        saramos_sched_housekeeping();

        saramos_process_t *p = ready_head;

        if (!p) {
            /* No ready process; keep ticking. */
            continue;
        }

        /* Skip the idle process if real work is available and idle is at head. */
        if (p == idle_proc && p->next_ready) {
            p = p->next_ready;
        }

        saramos_current_proc = p;
        p->state = SARAMOS_PROC_RUNNING;

        uint32_t quantum = (uint32_t)SARAMOS_SCHED_QUANTUM_MS * (uint32_t)p->weight;
        if (quantum == 0)
            quantum = 1;
        p->quantum_end_ms = saramos_tick_ms + quantum;

        uint32_t start_ms = saramos_tick_ms;
        int done = p->fn(p);
        uint32_t elapsed = saramos_tick_ms - start_ms;
        p->runtime_ms += elapsed;

        if (done || p->state == SARAMOS_PROC_ZOMBIE) {
            p->state = SARAMOS_PROC_ZOMBIE;
            saramos_sched_remove_ready(p);
            if (p->stdout_pipe)
                saramos_pipe_close(p->stdout_pipe);
            if (p->parent)
                saramos_proc_unblock(p->parent);
            continue;
        }

        if (p->state == SARAMOS_PROC_BLOCKED) {
            saramos_sched_remove_ready(p);
            continue;
        }

        /* Cooperative yield or quantum expired: rotate to tail. */
        p->state = SARAMOS_PROC_READY;
        rotate_ready(p);
    }
}
