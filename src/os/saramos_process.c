#include <os/saramos_process.h>
#include <os/saramos_scheduler.h>
#include <os/saramos_pipe.h>
#include <hal/board.h>
#include <string.h>
#include <stdio.h>

saramos_process_t *saramos_current_proc = NULL;

static saramos_process_t process_table[SARAMOS_MAX_PROCS];
static uint32_t next_proc_id = 1;

void saramos_proc_init(void)
{
    memset(process_table, 0, sizeof(process_table));
    next_proc_id = 1;
}

static saramos_process_t *proc_alloc(void)
{
    for (size_t i = 0; i < SARAMOS_MAX_PROCS; i++) {
        if (process_table[i].state == SARAMOS_PROC_UNUSED) {
            memset(&process_table[i], 0, sizeof(saramos_process_t));
            process_table[i].id = next_proc_id++;
            return &process_table[i];
        }
    }
    return NULL;
}

static void proc_attach_child(saramos_process_t *parent, saramos_process_t *child)
{
    if (!parent || !child)
        return;
    child->parent = parent;
    child->next_sibling = parent->child_head;
    parent->child_head = child;
}

static void proc_detach_child(saramos_process_t *parent, saramos_process_t *child)
{
    if (!parent || !child)
        return;
    saramos_process_t **pp = &parent->child_head;
    while (*pp) {
        if (*pp == child) {
            *pp = child->next_sibling;
            child->parent = NULL;
            child->next_sibling = NULL;
            return;
        }
        pp = &(*pp)->next_sibling;
    }
}

saramos_process_t *saramos_proc_spawn(const char *name,
                                      saramos_proc_fn_t fn,
                                      int argc,
                                      char **argv,
                                      struct saramos_pipe *in,
                                      struct saramos_pipe *out,
                                      uint8_t weight)
{
    return saramos_proc_spawn_child(saramos_current_proc, name, fn, argc, argv, in, out, weight);
}

saramos_process_t *saramos_proc_spawn_child(saramos_process_t *parent,
                                            const char *name,
                                            saramos_proc_fn_t fn,
                                            int argc,
                                            char **argv,
                                            struct saramos_pipe *in,
                                            struct saramos_pipe *out,
                                            uint8_t weight)
{
    saramos_process_t *p = proc_alloc();
    if (!p)
        return NULL;

    if (name) {
        strncpy(p->name, name, SARAMOS_PROC_NAME_LEN - 1);
        p->name[SARAMOS_PROC_NAME_LEN - 1] = '\0';
    }

    p->state = SARAMOS_PROC_READY;
    p->weight = weight ? weight : saramos_sched_weight_for_name(p->name);
    p->pt_line = 0;
    p->exit_code = 0;
    p->stdin_pipe = in;
    p->stdout_pipe = out;
    p->argc = argc;
    p->argv = argv;
    p->fn = fn;

    proc_attach_child(parent ? parent : NULL, p);
    saramos_sched_add_ready(p);
    return p;
}

void saramos_proc_block(saramos_process_t *p)
{
    if (!p)
        return;
    p->state = SARAMOS_PROC_BLOCKED;
    saramos_sched_remove_ready(p);
}

void saramos_proc_unblock(saramos_process_t *p)
{
    if (!p || p->state != SARAMOS_PROC_BLOCKED)
        return;
    p->state = SARAMOS_PROC_READY;
    saramos_sched_add_ready(p);
}

void saramos_proc_kill(saramos_process_t *p, int code)
{
    if (!p || p->state == SARAMOS_PROC_UNUSED)
        return;
    p->exit_code = code;
    p->state = SARAMOS_PROC_ZOMBIE;
    saramos_sched_remove_ready(p);
}

bool saramos_proc_has_children(const saramos_process_t *p)
{
    if (!p)
        return false;
    const saramos_process_t *c = p->child_head;
    while (c) {
        if (c->state != SARAMOS_PROC_UNUSED && c->state != SARAMOS_PROC_ZOMBIE)
            return true;
        c = c->next_sibling;
    }
    return false;
}

bool saramos_proc_wait_children(saramos_process_t *p)
{
    if (!p)
        return true;

    /* Reap finished children and check if any live children remain. */
    saramos_process_t **cp = &p->child_head;
    while (*cp) {
        saramos_process_t *c = *cp;
        if (c->state == SARAMOS_PROC_ZOMBIE) {
            *cp = c->next_sibling;
            c->state = SARAMOS_PROC_UNUSED;
            c->parent = NULL;
            c->next_sibling = NULL;
            continue;
        }
        cp = &c->next_sibling;
    }

    return !saramos_proc_has_children(p);
}

const char *saramos_proc_arg(const saramos_process_t *p, int idx)
{
    if (!p || idx < 0 || idx >= p->argc)
        return NULL;
    return p->argv[idx];
}

int proc_putc(char c)
{
    saramos_process_t *p = saramos_current_proc;
    if (p && p->stdout_pipe) {
        if (saramos_pipe_write_byte(p->stdout_pipe, (uint8_t)c) < 0)
            return -1;
    } else {
        hal_uart_putc(c);
    }
    return 0;
}

void proc_puts(const char *s)
{
    while (*s)
        proc_putc(*s++);
}

void proc_put_line(const char *s)
{
    proc_puts(s);
    proc_puts("\r\n");
}

int proc_try_getc(void)
{
    saramos_process_t *p = saramos_current_proc;
    if (p && p->stdin_pipe) {
        return saramos_pipe_read_byte(p->stdin_pipe);
    }
    return hal_uart_try_getc();
}

bool proc_input_eof(saramos_process_t *p)
{
    return p && p->stdin_pipe && saramos_pipe_eof(p->stdin_pipe);
}

void proc_wait_input(saramos_process_t *p)
{
    if (p && p->stdin_pipe)
        saramos_pipe_wait_reader(p->stdin_pipe, p);
}
