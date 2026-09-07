#ifndef SARAMOS_SEM_H
#define SARAMOS_SEM_H

#include <os/saramos_kernel.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Counting semaphore, backed by the same real TCB blocking as
 * saramos_mutex_t (saramos_task_block_current()/saramos_task_unblock())
 * -- a task waiting on saramos_sem_wait() is genuinely asleep, not
 * spinning.
 *
 * No priority inheritance here (unlike saramos_mutex_t): a semaphore
 * has no single "owner" task whose priority could meaningfully be
 * boosted -- inheritance only makes sense for a mutual-exclusion lock
 * with exactly one holder. Waiters are woken in priority order (the
 * highest-priority waiter first) on each post, same tie-breaking
 * approach as the mutex.
 */
typedef struct {
    uint32_t count;
    saramos_tcb_t *waiters[SARAMOS_MAX_TASKS];
    uint32_t waiter_count;
} saramos_sem_t;

/** @brief Initialize a semaphore with the given starting count. */
void saramos_sem_init(saramos_sem_t *s, uint32_t initial_count);

/**
 * @brief Wait (P/down): decrement the count, blocking the calling task
 * if it is already zero. Must be called from task context (not an ISR).
 */
void saramos_sem_wait(saramos_sem_t *s);

/**
 * @brief Non-blocking wait attempt.
 * @return true if the count was decremented, false if it was already
 *         zero (state unchanged).
 */
bool saramos_sem_trywait(saramos_sem_t *s);

/**
 * @brief Post (V/up): increment the count, or if a task is already
 * waiting, hand the unit straight to the highest-priority one instead
 * (equivalent, but avoids a spurious count>0-then-immediately-consumed
 * window). Safe to call from an ISR.
 */
void saramos_sem_post(saramos_sem_t *s);

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_SEM_H */
