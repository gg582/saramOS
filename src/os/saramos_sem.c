#include <os/saramos_sem.h>
#include <stdbool.h>

static inline uint32_t sem_irq_save(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask\n"
                   "cpsid i"
                   : "=r"(primask)
                   :
                   : "memory");
    return primask;
}

static inline void sem_irq_restore(uint32_t primask)
{
    __asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}

void saramos_sem_init(saramos_sem_t *s, uint32_t initial_count)
{
    if (!s)
        return;

    s->count = initial_count;
    s->waiter_count = 0;
    for (uint32_t i = 0; i < SARAMOS_MAX_TASKS; i++)
        s->waiters[i] = NULL;
}

static saramos_tcb_t *pop_highest_waiter(saramos_sem_t *s)
{
    uint32_t best_i = 0;
    saramos_tcb_t *best;

    if (s->waiter_count == 0)
        return NULL;

    for (uint32_t i = 1; i < s->waiter_count; i++) {
        if (s->waiters[i]->priority > s->waiters[best_i]->priority)
            best_i = i;
    }

    best = s->waiters[best_i];
    s->waiters[best_i] = s->waiters[s->waiter_count - 1];
    s->waiter_count--;
    return best;
}

void saramos_sem_wait(saramos_sem_t *s)
{
    uint32_t primask;
    saramos_tcb_t *self;

    if (!s)
        return;

    primask = sem_irq_save();

    if (s->count > 0) {
        s->count--;
        sem_irq_restore(primask);
        return;
    }

    self = saramos_task_self();
    if (self && s->waiter_count < SARAMOS_MAX_TASKS)
        s->waiters[s->waiter_count++] = self;

    sem_irq_restore(primask);

    /* Woken by saramos_sem_post() handing this task the unit directly
     * (count was not incremented in that case, so there is nothing left
     * to decrement here) -- see saramos_sem_post(). */
    saramos_task_block_current();
}

bool saramos_sem_trywait(saramos_sem_t *s)
{
    uint32_t primask;
    bool acquired = false;

    if (!s)
        return false;

    primask = sem_irq_save();
    if (s->count > 0) {
        s->count--;
        acquired = true;
    }
    sem_irq_restore(primask);

    return acquired;
}

void saramos_sem_post(saramos_sem_t *s)
{
    uint32_t primask;
    saramos_tcb_t *woken;

    if (!s)
        return;

    primask = sem_irq_save();

    woken = pop_highest_waiter(s);
    if (!woken) {
        /* No one waiting -- genuinely bank the unit for a future
         * wait(). */
        s->count++;
        sem_irq_restore(primask);
        return;
    }

    sem_irq_restore(primask);
    /* Hand the unit straight to the waiter that was already queued for
     * it, without ever making s->count visibly nonzero in between --
     * saramos_task_unblock() is ISR-safe and manages its own critical
     * section. */
    saramos_task_unblock(woken);
}
