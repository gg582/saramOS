#ifndef POSIX_PTHREAD_STUB_H
#define POSIX_PTHREAD_STUB_H

#ifdef TTAK_TARGET_ESP32

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef unsigned int pthread_t;
typedef int pthread_key_t;

typedef struct {
    volatile int state;
} pthread_mutex_t;

typedef struct {
    volatile int seq;
} pthread_cond_t;

typedef struct {
    volatile int done;
} pthread_once_t;

typedef struct {
    size_t stack_size;
    int detach_state;
} pthread_attr_t;

typedef struct {
    int type;
} pthread_mutexattr_t;

typedef struct {
    int kind;
} pthread_rwlockattr_t;

typedef struct {
    pthread_mutex_t lock;
} pthread_rwlock_t;

#define PTHREAD_MUTEX_INITIALIZER            {0}
#define PTHREAD_COND_INITIALIZER             {0}
#define PTHREAD_ONCE_INIT                    {0}
#define PTHREAD_RWLOCK_INITIALIZER           {{0}}
#define PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP 0

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

int pthread_cond_init(pthread_cond_t *cond, const void *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_setkind_np(pthread_rwlockattr_t *attr, int pref);

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);

pthread_t pthread_self(void);

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);
void *pthread_getspecific(pthread_key_t key);

#else
#include_next <pthread.h>
#endif

#endif /* POSIX_PTHREAD_STUB_H */
