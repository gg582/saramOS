#ifndef HAL_ESP32_WROOM32_H
#define HAL_ESP32_WROOM32_H

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include <signal.h>
#ifdef TTAK_TARGET_POSIX
#include <unistd.h>
#endif

/* POSIX sigjmp_buf compatibility */
#ifndef sigjmp_buf
    typedef jmp_buf sigjmp_buf;
#endif
#ifndef sigsetjmp
    #define sigsetjmp(env, save) setjmp(env)
#endif
#ifndef siglongjmp
    #define siglongjmp(env, val) longjmp(env, val)
#endif

/* POSIX Signal Stubs - Using a unique name to avoid conflicts */
#ifndef TTAK_TARGET_POSIX
#ifndef SA_RESTART
    #define SA_RESTART 0
#endif

typedef uint32_t ttak_sigset_t;
#define sigset_t ttak_sigset_t

#undef sigaction
#define sigaction(sig, act, oact) (0)
#define sigemptyset(set)          ((void)(set), 0)
#define sigismember(set, sig)     ((void)(set), (void)(sig), 0)
#define sigaddset(set, sig)       ((void)(set), (void)(sig), 0)

/* POSIX Priority Stubs */
#ifndef PRIO_PROCESS
    #define PRIO_PROCESS 0
#endif
static inline int setpriority(int which, int who, int prio) {
    (void)which; (void)who; (void)prio;
    return 0;
}

/* POSIX Memory Alignment Stub */
static inline int posix_memalign(void **memptr, size_t alignment, size_t size) {
    (void)alignment;
    if (!memptr) return 22; /* EINVAL */
    void *ptr = malloc(size);
    if (!ptr) return 12;    /* ENOMEM */
    *memptr = ptr;
    return 0;
}

/* File System Stubs */
#ifndef O_DIRECTORY
    #define O_DIRECTORY 0
#endif
static inline int fsync(int fd) {
    (void)fd;
    return 0;
}
#endif /* TTAK_TARGET_POSIX */

#ifdef __cplusplus
extern "C" {
#endif

/* 6. Hardware Abstraction Layer definitions */
typedef struct {
    unsigned int baud;
} esp32_hal_uart_t;

void esp32_hal_uart_init(esp32_hal_uart_t *dev, unsigned int baud);
void esp32_hal_uart_write(esp32_hal_uart_t *dev, const char *buf, size_t len);
void esp32_hal_tick_timer_start(uint32_t hz);
void esp32_hal_scheduler_pend(void);
void esp32_hal_isr_prologue(void);
void esp32_hal_isr_epilogue(void);
void esp32_hal_early_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ESP32_WROOM32_H */
