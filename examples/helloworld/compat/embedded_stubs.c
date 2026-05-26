#include <stdint.h>
#include <unistd.h>

#if defined(__XTENSA__)

uint64_t __atomic_fetch_add_8(volatile uint64_t *ptr, uint64_t val, int memorder) {
    (void)memorder;
    uint64_t prev = *ptr;
    *ptr = prev + val;
    return prev;
}

uint64_t __atomic_fetch_sub_8(volatile uint64_t *ptr, uint64_t val, int memorder) {
    (void)memorder;
    uint64_t prev = *ptr;
    *ptr = prev - val;
    return prev;
}

void __atomic_store_8(volatile uint64_t *ptr, uint64_t val, int memorder) {
    (void)memorder;
    *ptr = val;
}

uint64_t __atomic_load_8(const volatile uint64_t *ptr, int memorder) {
    (void)memorder;
    return *ptr;
}

long sysconf(int name) {
    switch (name) {
#ifdef _SC_PAGESIZE
        case _SC_PAGESIZE:
            return 4096;
#endif
        default:
            return -1;
    }
}

#endif /* __XTENSA__ */
