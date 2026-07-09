#ifndef SARAMOS_PROCESS_H
#define SARAMOS_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SARAMOS_MAX_PROCS 32U
#define SARAMOS_PROC_NAME_LEN 16U

/* Forward declarations */
struct saramos_pipe;
struct saramos_process;

typedef enum {
    SARAMOS_PROC_UNUSED = 0,
    SARAMOS_PROC_READY,
    SARAMOS_PROC_RUNNING,
    SARAMOS_PROC_BLOCKED,
    SARAMOS_PROC_ZOMBIE,
} saramos_proc_state_t;

typedef int (*saramos_proc_fn_t)(struct saramos_process *p);

typedef struct saramos_process {
    uint32_t id;
    char name[SARAMOS_PROC_NAME_LEN];
    saramos_proc_state_t state;
    uint8_t weight;

    /* Protothread continuation state. */
    uint16_t pt_line;
    int exit_code;

    /* Tree links. */
    struct saramos_process *parent;
    struct saramos_process *child_head;
    struct saramos_process *next_sibling;

    /* Ready list link. */
    struct saramos_process *next_ready;

    /* Block reason list link (pipe wait queues, sleep queue, etc.). */
    struct saramos_process *next_wait;

    /* I/O channels. */
    struct saramos_pipe *stdin_pipe;
    struct saramos_pipe *stdout_pipe;

    /* Arguments and private context. */
    int argc;
    char **argv;
    void *user_ctx;

    /* Time accounting. */
    uint32_t runtime_ms;
    uint32_t deadline_ms;   /* for sleep/wait timeouts */
    uint32_t quantum_end_ms;

    /* Persistent I/O iteration state for protothread macros. */
    struct {
        const char *out_ptr;
        size_t out_remain;
        const uint8_t *wr_ptr;
        size_t wr_remain;
        uint8_t *rd_ptr;
        size_t rd_remain;
        size_t rd_line_i;
    } io;

    /* Entry point. */
    saramos_proc_fn_t fn;
} saramos_process_t;

/* Protothread macros. Must be used inside the process function body. */
#define PROC_BEGIN(p)   switch ((p)->pt_line) { case 0:
#define _PROC_YIELD_LABEL(p, n) do { (p)->pt_line = (n); return 0; case (n):; } while (0)
#define PROC_YIELD(p)   _PROC_YIELD_LABEL(p, __COUNTER__ + 10000)
#define PROC_WAIT(p, cond) while (!(cond)) PROC_YIELD(p)
#define PROC_SLEEP_MS(p, ms) do { (p)->deadline_ms = saramos_tick_ms + (ms); \
                                   PROC_WAIT(p, saramos_tick_ms >= (p)->deadline_ms); } while (0)
#define PROC_EXIT(p, code) do { (p)->exit_code = (code); (p)->state = SARAMOS_PROC_ZOMBIE; return 1; } while (0)
#define PROC_END(p)     } (p)->state = SARAMOS_PROC_ZOMBIE; return 1

/* Global current process pointer (set by scheduler). */
extern saramos_process_t *saramos_current_proc;
extern volatile uint32_t saramos_tick_ms;

/* Process lifecycle. */
void saramos_proc_init(void);
saramos_process_t *saramos_proc_spawn(const char *name,
                                      saramos_proc_fn_t fn,
                                      int argc,
                                      char **argv,
                                      struct saramos_pipe *in,
                                      struct saramos_pipe *out,
                                      uint8_t weight);
saramos_process_t *saramos_proc_spawn_child(saramos_process_t *parent,
                                            const char *name,
                                            saramos_proc_fn_t fn,
                                            int argc,
                                            char **argv,
                                            struct saramos_pipe *in,
                                            struct saramos_pipe *out,
                                            uint8_t weight);
void saramos_proc_block(saramos_process_t *p);
void saramos_proc_unblock(saramos_process_t *p);
void saramos_proc_kill(saramos_process_t *p, int code);
bool saramos_proc_wait_children(saramos_process_t *p);
bool saramos_proc_has_children(const saramos_process_t *p);

/* Process argument helpers. */
const char *saramos_proc_arg(const saramos_process_t *p, int idx);

/* Process-aware I/O. Writes to stdout_pipe if present, otherwise UART. */
int proc_putc(char c);
void proc_puts(const char *s);
void proc_put_line(const char *s);

/* Blocking process output helpers that yield when the pipe is full. */
#define PROC_PUTC(p, c) \
    do { while (proc_putc(c) < 0) { \
             saramos_pipe_wait_writer((p)->stdout_pipe, (p)); \
             PROC_YIELD(p); } \
    } while (0)

#define PROC_PUTS(p, s) \
    do { (p)->io.out_ptr = (s); (p)->io.out_remain = strlen(s); \
         while ((p)->io.out_remain > 0) { \
             char _c = *(p)->io.out_ptr; \
             if (proc_putc(_c) < 0) { \
                 saramos_pipe_wait_writer((p)->stdout_pipe, (p)); \
                 PROC_YIELD(p); \
                 continue; \
             } \
             (p)->io.out_ptr++; \
             (p)->io.out_remain--; \
         } \
    } while (0)

#define PROC_PUT_LINE(p, s) \
    do { PROC_PUTS(p, s); \
         PROC_PUTS(p, "\r\n"); } while (0)

/* Blocking process input helpers that yield until data is available. */
#define PROC_GETC(p, out_c) \
    do { int _c; \
         while ((_c = proc_try_getc()) < 0) { \
             if (proc_input_eof(p)) break; \
             proc_wait_input(p); \
             PROC_YIELD(p); \
         } \
         (out_c) = _c; \
    } while (0)

#define PROC_WAIT_READABLE(p) \
    do { while (proc_try_getc() < 0) { \
             if (proc_input_eof(p)) break; \
             proc_wait_input(p); \
             PROC_YIELD(p); \
         } \
    } while (0)

/* Non-blocking input. Returns char or -1. Reads from stdin_pipe if present, otherwise UART. */
int proc_try_getc(void);
bool proc_input_eof(saramos_process_t *p);
void proc_wait_input(saramos_process_t *p);

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_PROCESS_H */
