#include <os/saramos_pipe.h>
#include <os/saramos_scheduler.h>
#include <string.h>

void saramos_pipe_init(saramos_pipe_t *pipe)
{
    if (!pipe)
        return;
    memset(pipe, 0, sizeof(*pipe));
    pipe->head = 0;
    pipe->tail = 0;
    pipe->count = 0;
    pipe->eof = false;
    pipe->writers = NULL;
    pipe->readers = NULL;
}

void saramos_pipe_close(saramos_pipe_t *pipe)
{
    if (!pipe)
        return;
    pipe->eof = true;
    saramos_pipe_wake_readers(pipe);
    saramos_pipe_wake_writers(pipe);
}

size_t saramos_pipe_space(const saramos_pipe_t *pipe)
{
    if (!pipe)
        return 0;
    return (size_t)(SARAMOS_PIPE_SIZE - pipe->count);
}

size_t saramos_pipe_avail(const saramos_pipe_t *pipe)
{
    if (!pipe)
        return 0;
    return (size_t)pipe->count;
}

bool saramos_pipe_eof(const saramos_pipe_t *pipe)
{
    if (!pipe)
        return true;
    return pipe->eof && pipe->count == 0;
}

int saramos_pipe_write_byte(saramos_pipe_t *pipe, uint8_t c)
{
    if (!pipe || pipe->count >= SARAMOS_PIPE_SIZE)
        return -1;
    pipe->buf[pipe->head] = c;
    pipe->head = (pipe->head + 1) % SARAMOS_PIPE_SIZE;
    pipe->count++;
    saramos_pipe_wake_readers(pipe);
    return 0;
}

int saramos_pipe_read_byte(saramos_pipe_t *pipe)
{
    if (!pipe || pipe->count == 0)
        return -1;
    uint8_t c = pipe->buf[pipe->tail];
    pipe->tail = (pipe->tail + 1) % SARAMOS_PIPE_SIZE;
    pipe->count--;
    saramos_pipe_wake_writers(pipe);
    return (int)c;
}

size_t saramos_pipe_write(saramos_pipe_t *pipe, const uint8_t *data, size_t len)
{
    if (!pipe || !data || len == 0 || pipe->count >= SARAMOS_PIPE_SIZE)
        return 0;

    size_t n = 0;
    while (n < len && pipe->count < SARAMOS_PIPE_SIZE) {
        pipe->buf[pipe->head] = data[n];
        pipe->head = (pipe->head + 1) % SARAMOS_PIPE_SIZE;
        pipe->count++;
        n++;
    }
    if (n > 0)
        saramos_pipe_wake_readers(pipe);
    return n;
}

size_t saramos_pipe_read(saramos_pipe_t *pipe, uint8_t *data, size_t len)
{
    if (!pipe || !data || len == 0 || pipe->count == 0)
        return 0;

    size_t n = 0;
    while (n < len && pipe->count > 0) {
        data[n] = pipe->buf[pipe->tail];
        pipe->tail = (pipe->tail + 1) % SARAMOS_PIPE_SIZE;
        pipe->count--;
        n++;
    }
    if (n > 0)
        saramos_pipe_wake_writers(pipe);
    return n;
}

static void enqueue_waiter(saramos_process_t **list, saramos_process_t *p)
{
    if (!list || !p)
        return;
    p->next_wait = *list;
    *list = p;
    saramos_sched_block(p);
}

static void wake_list(saramos_process_t **list)
{
    while (*list) {
        saramos_process_t *p = *list;
        *list = p->next_wait;
        p->next_wait = NULL;
        saramos_sched_unblock(p);
    }
}

void saramos_pipe_wait_writer(saramos_pipe_t *pipe, saramos_process_t *p)
{
    if (!pipe || !p)
        return;
    enqueue_waiter(&pipe->writers, p);
}

void saramos_pipe_wait_reader(saramos_pipe_t *pipe, saramos_process_t *p)
{
    if (!pipe || !p)
        return;
    enqueue_waiter(&pipe->readers, p);
}

void saramos_pipe_wake_writers(saramos_pipe_t *pipe)
{
    if (!pipe)
        return;
    wake_list(&pipe->writers);
}

void saramos_pipe_wake_readers(saramos_pipe_t *pipe)
{
    if (!pipe)
        return;
    wake_list(&pipe->readers);
}
