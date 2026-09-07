#include <os/saramos_mutex.h>

static inline uint32_t mtx_irq_save(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask\n"
                   "cpsid i"
                   : "=r"(primask)
                   :
                   : "memory");
    return primask;
}

static inline void mtx_irq_restore(uint32_t primask)
{
    __asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}

void saramos_mutex_init(saramos_mutex_t *m)
{
    if (!m)
        return;

    m->owner = NULL;
    m->owner_base_priority = 0;
    m->boosted = false;
    m->waiter_count = 0;
    for (uint32_t i = 0; i < SARAMOS_MAX_TASKS; i++)
        m->waiters[i] = NULL;
}

/* Highest-priority waiter currently queued, or NULL. Linear scan is
 * fine here: SARAMOS_MAX_TASKS is small (16) and this only runs on the
 * lock/unlock slow paths, never per-tick. */
static saramos_tcb_t *highest_waiter(const saramos_mutex_t *m)
{
    saramos_tcb_t *best = NULL;

    for (uint32_t i = 0; i < m->waiter_count; i++) {
        saramos_tcb_t *w = m->waiters[i];
        if (!best || w->priority > best->priority)
            best = w;
    }
    return best;
}

static void waiter_remove(saramos_mutex_t *m, saramos_tcb_t *tcb)
{
    for (uint32_t i = 0; i < m->waiter_count; i++) {
        if (m->waiters[i] == tcb) {
            m->waiters[i] = m->waiters[m->waiter_count - 1];
            m->waiter_count--;
            return;
        }
    }
}

/* Apply/refresh priority inheritance on the current owner given the
 * current waiter set. Called any time the waiter set changes (a new
 * waiter joins, or the owner is about to change). Only ever RAISES the
 * owner above its own base priority -- restoring it back down is
 * unlock's job (see saramos_mutex_unlock()), since a still-held mutex
 * must keep boosting its owner for as long as a higher-priority task is
 * waiting on it, even if other waiters come and go. */
static void reinherit(saramos_mutex_t *m)
{
    saramos_tcb_t *top;

    if (!m->owner)
        return;

    top = highest_waiter(m);
    if (!top)
        return;

    if (!m->boosted) {
        if (top->priority > m->owner->priority) {
            m->owner_base_priority = m->owner->priority;
            m->boosted = true;
            saramos_task_set_priority(m->owner, top->priority);
        }
    } else if (top->priority > m->owner->priority) {
        saramos_task_set_priority(m->owner, top->priority);
    }
}

void saramos_mutex_lock(saramos_mutex_t *m)
{
    uint32_t primask;
    saramos_tcb_t *self;

    if (!m)
        return;

    primask = mtx_irq_save();

    if (!m->owner) {
        m->owner = saramos_task_self();
        m->boosted = false;
        mtx_irq_restore(primask);
        return;
    }

    self = saramos_task_self();
    if (self && m->waiter_count < SARAMOS_MAX_TASKS)
        m->waiters[m->waiter_count++] = self;
    reinherit(m);

    mtx_irq_restore(primask);

    /* saramos_task_block_current() manages its own critical section and
     * only returns once saramos_mutex_unlock() has handed this task the
     * mutex (via saramos_task_unblock()) and it has actually been
     * rescheduled -- at that point m->owner == self already, set by
     * unlock(), so there is nothing left to do here. */
    saramos_task_block_current();
}

bool saramos_mutex_trylock(saramos_mutex_t *m)
{
    uint32_t primask;
    bool acquired = false;

    if (!m)
        return false;

    primask = mtx_irq_save();
    if (!m->owner) {
        m->owner = saramos_task_self();
        m->boosted = false;
        acquired = true;
    }
    mtx_irq_restore(primask);

    return acquired;
}

void saramos_mutex_unlock(saramos_mutex_t *m)
{
    uint32_t primask;
    saramos_tcb_t *releasing;
    saramos_tcb_t *next_owner;

    if (!m)
        return;

    primask = mtx_irq_save();

    releasing = m->owner;
    if (releasing && m->boosted) {
        saramos_task_set_priority(releasing, m->owner_base_priority);
        m->boosted = false;
    }

    next_owner = highest_waiter(m);
    if (next_owner) {
        waiter_remove(m, next_owner);
        m->owner = next_owner;
        /* The new owner may still have to inherit from whoever is left
         * waiting behind it. */
        reinherit(m);
        mtx_irq_restore(primask);
        /* saramos_task_unblock() is safe to call with interrupts
         * already restored -- it manages its own critical section. */
        saramos_task_unblock(next_owner);
        return;
    }

    m->owner = NULL;
    mtx_irq_restore(primask);
}
