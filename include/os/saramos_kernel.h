#ifndef SARAMOS_KERNEL_H
#define SARAMOS_KERNEL_H

#include <os/saramos_arena.h>
#include <os/saramos_owner.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SARAMOS_MAX_TASKS 16U
#define SARAMOS_TASK_ID_INVALID UINT32_MAX

typedef void (*saramos_task_entry_t)(void *arg);

typedef enum {
    SARAMOS_TASK_UNUSED = 0,
    SARAMOS_TASK_READY,
    SARAMOS_TASK_RUNNING,
    SARAMOS_TASK_BLOCKED,
    SARAMOS_TASK_KILLED,
    SARAMOS_TASK_FAULTED
} saramos_task_state_t;

typedef struct saramos_tcb {
    uint32_t *sp;
    saramos_task_state_t state;
    uint8_t priority;
    uint32_t task_id;
    saramos_owner_t *owner_ctx;
    saramos_arena_t *bound_arena;
    uint32_t bound_epoch;
    uint32_t fault_count;
    struct saramos_tcb *next_ready;
} saramos_tcb_t;

extern saramos_tcb_t *saramos_current_tcb;
extern saramos_tcb_t *saramos_next_tcb;

void saramos_kernel_init(void);

bool saramos_task_init(saramos_tcb_t *tcb,
                       uint32_t task_id,
                       saramos_task_entry_t entry,
                       void *arg,
                       void *stack_mem,
                       size_t stack_bytes,
                       uint8_t priority,
                       saramos_owner_t *owner_ctx,
                       saramos_arena_t *bound_arena);

bool saramos_task_add(saramos_tcb_t *tcb);
saramos_tcb_t *saramos_task_find(uint32_t task_id);
saramos_tcb_t *saramos_task_self(void);
bool saramos_task_validate_bindings(const saramos_tcb_t *tcb);

void saramos_kernel_start(void) __attribute__((noreturn));
void saramos_schedule(void);
void saramos_task_yield(void);
void saramos_task_exit(void) __attribute__((noreturn));
void saramos_task_kill_and_reclaim(uint32_t task_id);

/* Block the calling task (mark it SARAMOS_TASK_BLOCKED, remove it from
 * the ready list) and switch to the next ready task, exactly like
 * saramos_schedule() except the caller does not go back on the ready
 * list -- something else must call saramos_task_unblock() on it later
 * to make it schedulable again. Used by synchronization primitives
 * (saramos_mutex_t, saramos_sem_t) to actually sleep a task instead of
 * busy-waiting. Must be called from task context (not an ISR), with
 * interrupts enabled -- it manages its own critical section. */
void saramos_task_block_current(void);

/* Move a SARAMOS_TASK_BLOCKED task back onto the ready list. Safe to
 * call from any context, including ISRs. A no-op if tcb is NULL or not
 * currently blocked (so it's safe to call speculatively). */
void saramos_task_unblock(saramos_tcb_t *tcb);

/* Change a task's priority, re-positioning it in the ready list if it
 * is currently on it. Used by saramos_mutex_t for priority inheritance
 * (temporarily raising a lock owner's priority to the highest blocked
 * waiter's, then restoring it on unlock). */
void saramos_task_set_priority(saramos_tcb_t *tcb, uint8_t priority);
void saramos_hardfault_dispatch(uint32_t *fault_stack, uint32_t exc_return) __attribute__((noreturn));

void saramos_start_tcb(saramos_tcb_t *tcb) __attribute__((noreturn));
void saramos_fault_switch_to(saramos_tcb_t *tcb) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_KERNEL_H */
