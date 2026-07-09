#include "os/saramos_kernel.h"
#include <hal/board.h>
#include <stdio.h>

#define SARAMOS_ICSR_ADDR 0xE000ED04UL
#define SARAMOS_ICSR_PENDSVSET (1UL << 28)

#define SARAMOS_XPSR_T_BIT (1UL << 24)
#define SARAMOS_STACK_ALIGN_MASK 0x7UL

volatile uint32_t *const saramos_icsr = (volatile uint32_t *)SARAMOS_ICSR_ADDR;

saramos_tcb_t *saramos_current_tcb;
saramos_tcb_t *saramos_next_tcb;

static saramos_tcb_t *task_table[SARAMOS_MAX_TASKS];
static saramos_tcb_t *ready_head;
static uint32_t task_table_count;

static inline uint32_t saramos_irq_save(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask\n"
                   "cpsid i"
                   : "=r"(primask)
                   :
                   : "memory");
    return primask;
}

static inline void saramos_irq_restore(uint32_t primask)
{
    __asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}

static inline void saramos_barrier(void)
{
    __asm volatile("dsb\n"
                   "isb"
                   :
                   :
                   : "memory");
}

static void ready_remove_locked(saramos_tcb_t *tcb)
{
    if (!tcb || !ready_head)
        return;

    saramos_tcb_t *prev = NULL;
    saramos_tcb_t *cur = ready_head;

    while (cur) {
        if (cur == tcb) {
            if (prev)
                prev->next_ready = cur->next_ready;
            else
                ready_head = cur->next_ready;
            cur->next_ready = NULL;
            return;
        }
        prev = cur;
        cur = cur->next_ready;
    }
}

static bool ready_contains_locked(const saramos_tcb_t *tcb)
{
    const saramos_tcb_t *cur = ready_head;

    while (cur) {
        if (cur == tcb)
            return true;
        cur = cur->next_ready;
    }

    return false;
}

static void ready_insert_locked(saramos_tcb_t *tcb)
{
    if (!tcb || ready_contains_locked(tcb))
        return;

    tcb->next_ready = NULL;

    if (!ready_head || tcb->priority > ready_head->priority) {
        tcb->next_ready = ready_head;
        ready_head = tcb;
        return;
    }

    saramos_tcb_t *cur = ready_head;
    while (cur->next_ready && cur->next_ready->priority >= tcb->priority)
        cur = cur->next_ready;

    tcb->next_ready = cur->next_ready;
    cur->next_ready = tcb;
}

static saramos_tcb_t *pick_next_locked(void)
{
    saramos_tcb_t *candidate = ready_head;

    while (candidate) {
        if (candidate->state == SARAMOS_TASK_READY)
            return candidate;
        candidate = candidate->next_ready;
    }

    return NULL;
}

static void reclaim_task_resources(saramos_tcb_t *tcb)
{
    saramos_owner_t *owner;
    saramos_arena_t *arena;

    if (!tcb)
        return;

    owner = tcb->owner_ctx;
    arena = tcb->bound_arena;

    tcb->owner_ctx = NULL;
    tcb->bound_arena = NULL;
    tcb->bound_epoch = 0;

    if (owner)
        saramos_owner_destroy(owner);

    if (arena) {
        if (arena->generation_active)
            saramos_arena_rotate(arena);
        else
            saramos_arena_reset(arena);
    }
}

void saramos_kernel_init(void)
{
    uint32_t primask = saramos_irq_save();

    for (uint32_t i = 0; i < SARAMOS_MAX_TASKS; i++)
        task_table[i] = NULL;

    task_table_count = 0;
    ready_head = NULL;
    saramos_current_tcb = NULL;
    saramos_next_tcb = NULL;

    saramos_irq_restore(primask);
}

bool saramos_task_init(saramos_tcb_t *tcb,
                       uint32_t task_id,
                       saramos_task_entry_t entry,
                       void *arg,
                       void *stack_mem,
                       size_t stack_bytes,
                       uint8_t priority,
                       saramos_owner_t *owner_ctx,
                       saramos_arena_t *bound_arena)
{
    uint32_t *sp;

    if (!tcb || !entry || !stack_mem || stack_bytes < (16U * sizeof(uint32_t)))
        return false;

    sp = (uint32_t *)((uintptr_t)stack_mem + stack_bytes);
    sp = (uint32_t *)((uintptr_t)sp & ~SARAMOS_STACK_ALIGN_MASK);

    *(--sp) = SARAMOS_XPSR_T_BIT;
    *(--sp) = ((uint32_t)(uintptr_t)entry) | 1U;
    *(--sp) = (uint32_t)(uintptr_t)saramos_task_exit;
    *(--sp) = 0x12121212UL;
    *(--sp) = 0x03030303UL;
    *(--sp) = 0x02020202UL;
    *(--sp) = 0x01010101UL;
    *(--sp) = (uint32_t)(uintptr_t)arg;

    *(--sp) = 0x11111111UL;
    *(--sp) = 0x10101010UL;
    *(--sp) = 0x09090909UL;
    *(--sp) = 0x08080808UL;
    *(--sp) = 0x07070707UL;
    *(--sp) = 0x06060606UL;
    *(--sp) = 0x05050505UL;
    *(--sp) = 0x04040404UL;

    tcb->sp = sp;
    tcb->state = SARAMOS_TASK_READY;
    tcb->priority = priority;
    tcb->task_id = task_id;
    tcb->owner_ctx = owner_ctx;
    tcb->bound_arena = bound_arena;
    tcb->bound_epoch = bound_arena ? bound_arena->epoch_counter : 0;
    tcb->fault_count = 0;
    tcb->next_ready = NULL;

    return true;
}

bool saramos_task_add(saramos_tcb_t *tcb)
{
    uint32_t primask;

    if (!tcb || tcb->state == SARAMOS_TASK_UNUSED)
        return false;

    primask = saramos_irq_save();

    if (task_table_count >= SARAMOS_MAX_TASKS || saramos_task_find(tcb->task_id)) {
        saramos_irq_restore(primask);
        return false;
    }

    task_table[task_table_count++] = tcb;
    tcb->state = SARAMOS_TASK_READY;
    ready_insert_locked(tcb);

    saramos_irq_restore(primask);
    return true;
}

saramos_tcb_t *saramos_task_find(uint32_t task_id)
{
    for (uint32_t i = 0; i < task_table_count; i++) {
        saramos_tcb_t *tcb = task_table[i];
        if (tcb && tcb->task_id == task_id)
            return tcb;
    }

    return NULL;
}

saramos_tcb_t *saramos_task_self(void)
{
    return saramos_current_tcb;
}

bool saramos_task_validate_bindings(const saramos_tcb_t *tcb)
{
    if (!tcb)
        return false;

    if (tcb->owner_ctx && !tcb->owner_ctx->owner)
        return false;

    if (tcb->bound_arena && tcb->bound_arena->epoch_counter != tcb->bound_epoch)
        return false;

    return true;
}

void saramos_kernel_start(void)
{
    uint32_t primask = saramos_irq_save();
    saramos_tcb_t *next = pick_next_locked();

    if (!next) {
        saramos_irq_restore(primask);
        for (;;)
            __asm volatile("wfi");
    }

    ready_remove_locked(next);
    next->state = SARAMOS_TASK_RUNNING;
    saramos_current_tcb = next;
    saramos_next_tcb = next;
    saramos_irq_restore(primask);

    saramos_start_tcb(next);
}

void saramos_schedule(void)
{
    uint32_t primask = saramos_irq_save();
    saramos_tcb_t *current = saramos_current_tcb;
    saramos_tcb_t *next;

    if (current && current->state == SARAMOS_TASK_RUNNING) {
        current->state = SARAMOS_TASK_READY;
        ready_insert_locked(current);
    }

    next = pick_next_locked();
    if (next) {
        ready_remove_locked(next);
        next->state = SARAMOS_TASK_RUNNING;
        saramos_next_tcb = next;
        *saramos_icsr = SARAMOS_ICSR_PENDSVSET;
        saramos_barrier();
    }

    saramos_irq_restore(primask);
}

void saramos_task_yield(void)
{
    saramos_schedule();
}

void saramos_task_exit(void)
{
    saramos_tcb_t *self = saramos_current_tcb;

    if (self)
        saramos_task_kill_and_reclaim(self->task_id);

    saramos_schedule();

    for (;;)
        __asm volatile("wfi");
}

void saramos_task_kill_and_reclaim(uint32_t task_id)
{
    uint32_t primask = saramos_irq_save();
    saramos_tcb_t *tcb = saramos_task_find(task_id);

    if (!tcb) {
        saramos_irq_restore(primask);
        return;
    }

    ready_remove_locked(tcb);
    if (tcb == saramos_current_tcb)
        saramos_current_tcb = NULL;

    tcb->state = SARAMOS_TASK_KILLED;
    tcb->fault_count++;
    reclaim_task_resources(tcb);
    tcb->sp = NULL;

    saramos_irq_restore(primask);
}

void saramos_hardfault_dispatch(uint32_t *fault_stack, uint32_t exc_return)
{
    saramos_tcb_t *faulted = saramos_current_tcb;
    saramos_tcb_t *next;
    uint32_t primask;
    char buf[80];

    (void)exc_return;

    hal_uart_puts("\r\n*** HardFault ***\r\n");
    if (fault_stack) {
        snprintf(buf, sizeof(buf),
                 "r0=%08lx r1=%08lx r2=%08lx r3=%08lx\r\n",
                 (unsigned long)fault_stack[0], (unsigned long)fault_stack[1],
                 (unsigned long)fault_stack[2], (unsigned long)fault_stack[3]);
        hal_uart_puts(buf);
        snprintf(buf, sizeof(buf),
                 "r12=%08lx lr=%08lx pc=%08lx xpsr=%08lx\r\n",
                 (unsigned long)fault_stack[4], (unsigned long)fault_stack[5],
                 (unsigned long)fault_stack[6], (unsigned long)fault_stack[7]);
        hal_uart_puts(buf);
    }

    primask = saramos_irq_save();

    if (faulted) {
        faulted->state = SARAMOS_TASK_FAULTED;
        faulted->fault_count++;
        ready_remove_locked(faulted);
        saramos_current_tcb = NULL;
        reclaim_task_resources(faulted);
        faulted->sp = NULL;
    }

    next = pick_next_locked();
    if (next) {
        ready_remove_locked(next);
        next->state = SARAMOS_TASK_RUNNING;
        saramos_current_tcb = next;
        saramos_next_tcb = next;
        saramos_irq_restore(primask);
        saramos_fault_switch_to(next);
    }

    saramos_irq_restore(primask);

    for (;;)
        __asm volatile("wfi");
}
