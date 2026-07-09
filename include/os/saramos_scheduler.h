#ifndef SARAMOS_SCHEDULER_H
#define SARAMOS_SCHEDULER_H

#include <stdint.h>
#include <os/saramos_process.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Base time slice in milliseconds. */
#ifndef SARAMOS_SCHED_QUANTUM_MS
#define SARAMOS_SCHED_QUANTUM_MS   10U
#endif

/* Scheduling weights. */
#ifndef SARAMOS_SCHED_WEIGHT_NORMAL
#define SARAMOS_SCHED_WEIGHT_NORMAL 1U
#endif

#ifndef SARAMOS_SCHED_WEIGHT_CORE
#define SARAMOS_SCHED_WEIGHT_CORE   2U
#endif

/* Names that mark a process as core I/O. */
#ifndef SARAMOS_SCHED_CORE_PREFIXES
#define SARAMOS_SCHED_CORE_PREFIXES "sd", "net", "http", "flash"
#endif

/* Scheduler API. */
void saramos_sched_init(void);
void saramos_sched_run(void) __attribute__((noreturn));
void saramos_sched_yield(void);
void saramos_sched_block(saramos_process_t *p);
void saramos_sched_unblock(saramos_process_t *p);
void saramos_sched_remove_ready(saramos_process_t *p);
void saramos_sched_add_ready(saramos_process_t *p);

/* Determine weight from process name. */
uint8_t saramos_sched_weight_for_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_SCHEDULER_H */
