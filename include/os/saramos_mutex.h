#ifndef SARAMOS_MUTEX_H
#define SARAMOS_MUTEX_H

#include <os/saramos_kernel.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Priority-inheritance mutex, backed by saramos_kernel.c's real
 * TCB/PendSV task blocking (saramos_task_block_current()/
 * saramos_task_unblock()) -- a task waiting on a locked mutex is truly
 * asleep (SARAMOS_TASK_BLOCKED, off the ready list), not spinning.
 *
 * Priority inheritance: while a higher-priority task is blocked waiting
 * for this mutex, the current owner's priority is temporarily raised to
 * match (the classic fix for unbounded priority inversion -- a low-
 * priority owner holding a lock a high-priority task needs must not sit
 * behind medium-priority tasks that never touch the mutex at all). The
 * owner's original priority is restored the moment it releases the
 * mutex. This implementation supports a single inheritance level per
 * mutex (the boost tracks the single highest current waiter); nested
 * inheritance chains across multiple mutexes held simultaneously are
 * not specially handled beyond what naturally falls out of each mutex
 * independently boosting/restoring its own owner.
 */
typedef struct {
    saramos_tcb_t *owner;          /* NULL if unlocked */
    uint8_t owner_base_priority;   /* owner's priority before any boost
                                     * from THIS mutex; only meaningful
                                     * while boosted is true */
    bool boosted;

    /* Fixed-size waiter set instead of a linked list through the TCB:
     * saramos_tcb_t has exactly one link field (next_ready), already
     * owned by the kernel's ready list and guaranteed unused while a
     * task is BLOCKED, but reusing it here would couple this module to
     * that internal invariant. SARAMOS_MAX_TASKS is already the
     * system-wide bound on live tasks, so a plain array costs little
     * and keeps the two modules decoupled. */
    saramos_tcb_t *waiters[SARAMOS_MAX_TASKS];
    uint32_t waiter_count;
} saramos_mutex_t;

/** @brief Initialize a mutex to the unlocked state. */
void saramos_mutex_init(saramos_mutex_t *m);

/**
 * @brief Lock the mutex, blocking the calling task if it is already
 * held. Must be called from task context (not an ISR).
 */
void saramos_mutex_lock(saramos_mutex_t *m);

/**
 * @brief Non-blocking lock attempt.
 * @return true if the mutex was free and is now held by the caller,
 *         false if it was already locked (state unchanged).
 */
bool saramos_mutex_trylock(saramos_mutex_t *m);

/**
 * @brief Unlock the mutex. Undefined behavior if the calling task does
 * not currently hold it (mirrors typical RTOS mutex semantics -- no
 * ownership check is performed, to keep the fast path cheap).
 */
void saramos_mutex_unlock(saramos_mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_MUTEX_H */
