#include <pthread.h>

#ifdef TTAK_TARGET_ESP32

#include <errno.h>

#define PTHREAD_STUB_TLS_MAX 32

typedef void *(*pthread_start_routine_t)(void *);

static void *g_tls_values[PTHREAD_STUB_TLS_MAX];
static struct {
    int used;
    void (*destructor)(void *);
} g_tls_meta[PTHREAD_STUB_TLS_MAX];

static inline void pthread_stub_relax(void) {
    __asm__ __volatile__("nop");
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)attr;
    if (!mutex) return EINVAL;
    mutex->state = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    while (__atomic_test_and_set(&mutex->state, __ATOMIC_ACQUIRE)) {
        pthread_stub_relax();
    }
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    return __atomic_test_and_set(&mutex->state, __ATOMIC_ACQUIRE) ? EBUSY : 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    __atomic_clear(&mutex->state, __ATOMIC_RELEASE);
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (!attr) return EINVAL;
    attr->type = 0;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    (void)attr;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (!attr) return EINVAL;
    attr->type = type;
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const void *attr) {
    (void)attr;
    if (!cond) return EINVAL;
    cond->seq = 0;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    (void)cond;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (!cond || !mutex) return EINVAL;
    int seq = __atomic_load_n(&cond->seq, __ATOMIC_ACQUIRE);
    pthread_mutex_unlock(mutex);
    while (__atomic_load_n(&cond->seq, __ATOMIC_ACQUIRE) == seq) {
        pthread_stub_relax();
    }
    pthread_mutex_lock(mutex);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    (void)abstime;
    return pthread_cond_wait(cond, mutex);
}

int pthread_cond_signal(pthread_cond_t *cond) {
    if (!cond) return EINVAL;
    __atomic_fetch_add(&cond->seq, 1, __ATOMIC_RELEASE);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    return pthread_cond_signal(cond);
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) return EINVAL;
    if (!__atomic_load_n(&once_control->done, __ATOMIC_ACQUIRE)) {
        init_routine();
        __atomic_store_n(&once_control->done, 1, __ATOMIC_RELEASE);
    }
    return 0;
}

int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr) {
    (void)attr;
    if (!rwlock) return EINVAL;
    return pthread_mutex_init(&rwlock->lock, NULL);
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock) {
    if (!rwlock) return EINVAL;
    return pthread_mutex_destroy(&rwlock->lock);
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) {
    if (!rwlock) return EINVAL;
    return pthread_mutex_lock(&rwlock->lock);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) {
    if (!rwlock) return EINVAL;
    return pthread_mutex_lock(&rwlock->lock);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) {
    if (!rwlock) return EINVAL;
    return pthread_mutex_unlock(&rwlock->lock);
}

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) {
    if (!attr) return EINVAL;
    attr->kind = 0;
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) {
    (void)attr;
    return 0;
}

int pthread_rwlockattr_setkind_np(pthread_rwlockattr_t *attr, int pref) {
    if (!attr) return EINVAL;
    attr->kind = pref;
    return 0;
}

int pthread_attr_init(pthread_attr_t *attr) {
    if (!attr) return EINVAL;
    attr->stack_size = 0;
    attr->detach_state = 0;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    (void)attr;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    if (!attr) return EINVAL;
    attr->stack_size = stacksize;
    return 0;
}

static pthread_t pthread_stub_next_id(void) {
    static unsigned int counter = 1;
    return counter++;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    (void)attr;
    if (!start_routine) return EINVAL;
    if (thread) {
        *thread = pthread_stub_next_id();
    }
    start_routine(arg);
    return 0;
}

int pthread_join(pthread_t thread, void **retval) {
    (void)thread;
    if (retval) {
        *retval = NULL;
    }
    return 0;
}

pthread_t pthread_self(void) {
    return 1;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    if (!key) return EINVAL;
    for (int i = 0; i < PTHREAD_STUB_TLS_MAX; ++i) {
        if (!g_tls_meta[i].used) {
            g_tls_meta[i].used = 1;
            g_tls_meta[i].destructor = destructor;
            g_tls_values[i] = NULL;
            *key = i;
            return 0;
        }
    }
    return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
    if (key < 0 || key >= PTHREAD_STUB_TLS_MAX) return EINVAL;
    g_tls_meta[key].used = 0;
    g_tls_meta[key].destructor = NULL;
    g_tls_values[key] = NULL;
    return 0;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    if (key < 0 || key >= PTHREAD_STUB_TLS_MAX || !g_tls_meta[key].used) return EINVAL;
    g_tls_values[key] = (void *)value;
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    if (key < 0 || key >= PTHREAD_STUB_TLS_MAX || !g_tls_meta[key].used) return NULL;
    return g_tls_values[key];
}

#endif /* TTAK_TARGET_ESP32 */
