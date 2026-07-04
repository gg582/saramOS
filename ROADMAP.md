# saramOS libttak POSIX-Free Bare-Metal Refactoring ROADMAP

## Objective
Transform `engine/libttak` into a fully POSIX-free, bare-metal capable engine for STM32F769I-DISC1 (Cortex-M7). No pthread, no dynamic malloc/free, no generic OS libc dependencies.

---

## Milestone 1: Isolation of Standard Libraries & Abstracting Synchronizations
**Goal:** Eliminate all direct libc dynamic allocation and POSIX threading primitives.

- [x] Audit every `malloc`, `calloc`, `realloc`, `free` call in `src/` and `internal/`.
- [x] Redirect all dynamic allocation paths to `ttak_mem_pocket` static pools via `baremetal_alloc.c`.
- [x] Provide bare-metal POSIX compatibility shim (`engine/libttak/include/pthread.h`) with interrupt-disable mutexes.
- [x] Provide `ttak_apply_mols_control` stub in `baremetal_alloc.c` (src/net excluded from bare-metal build).
- [ ] Define opaque engine-native synchronization types (`ttak_mutex_t`, `ttak_cond_t`, `ttak_thread_t`).
- [ ] Replace all `pthread_*` references with engine-native abstraction layer.
- [ ] Guard POSIX implementations under `#if !defined(EMBEDDED)` or `#if defined(TTAK_HOSTED)`.

> **Status:** Functional. All POSIX deps are stubbed/shimmed for bare-metal. Full abstraction refactor deferred.

---

## Milestone 2: POSIX-Free Core Worker & Threading Model Refactoring
**Goal:** Remove `pthread_create`, `pthread_join`, and any multi-threaded worker assumptions from bare-metal builds.

- [x] Strip `pthread_create` and `pthread_join` from `src/thread/worker.c` via shim (return ENOTSUP).
- [x] Implement single-thread stubs: `pthread_self() == 1`, `pthread_mutex_lock` disables interrupts.
- [x] Provide 64-bit atomic software fallbacks in `src/atomic/atomic.c` (global lock, interrupt-disable).
- [x] Strip `_Thread_local` via `-D_Thread_local=`; epoch TLS degrades to single global state.
- [ ] Implement single cooperative worker loop for `EMBEDDED` builds (no preemption, no threads).
- [ ] Ensure `#if defined(EMBEDDED)` blocks contain zero POSIX headers.

> **Status:** Linker clean. Runtime verified. Full cooperative scheduler deferred.

---

## Milestone 3: Embedded Optimization for Memory Pocket & VMA
**Goal:** Fix memory layout so all allocations live in static compile-time pools mapped to STM32 SRAM.

- [x] Hard-code `TTAK_VMA_REGION_SIZE` and mapping offsets to STM32F769I internal SRAM regions in `internal/ttak/mem_internal.h` using `#if defined(EMBEDDED_BAREMETAL)` guards.
- [x] Convert pocket page pool from any dynamic mapping to a compile-time static array placed in BSS.
- [x] Ensure `ttak_mem_alloc` / `ttak_mem_free` operate only on static pools (no heap walk).
- [x] Validate that no allocation path exceeds 512KB SRAM limit.

**Final pool sizes (bare-metal):**
| Pool | Size |
|------|------|
| `buddy_pool` | 32 KB (`TTAK_EMBEDDED_POOL_ORDER = 15`) |
| `pocket_page_pool` | 32 KB (`TTAK_POCKET_POOL_SIZE = 4096 * 8`) |
| `vma_region_buffer` | 16 KB (`TTAK_VMA_REGION_SIZE = 16K`) |
| `large_region_buffer` | 16 KB (`TTAK_LARGE_REGION_SIZE = 16K`) |
| `g_baremetal_heap` | 64 KB |
| **Other .bss** | ~14 KB |
| **Total .bss** | ~174 KB |

> **Status:** Complete. `.bss` fits comfortably in 512KB RAM.

---

## Milestone 4: saramOS Integration & Target Flashing
**Goal:** Produce a flashable binary where the engine initializes, allocates, and loops without HardFault.

- [x] Update `os/default/Makefile` to cross-compile `libttak` with `arm-none-eabi-gcc`.
- [x] Ensure CFLAGS include `-mcpu=cortex-m7`, `-mthumb`, `-mfloat-abi=hard`, `-mfpu=fpv5-sp-d16`, `-fno-lto`.
- [x] Build final ELF/BIN with zero undefined POSIX symbols.
- [x] Flash onto STM32F769I-DISC1 via OpenOCD.
- [x] Verify via UART:
  - Engine init success log
  - Allocation survival log (static pool only)
  - System heartbeat loop without HardFault

**Verified UART output:**
```
=== saramOS on STM32F769I-DISC1 ===
libttak: alloc OK (64 bytes)
libttak: free OK
Hello World
===================================
Heartbeat from saramOS
Heartbeat from saramOS
...
```

> **Status:** Complete. Binary flashes and runs correctly.

---

## Remaining Technical Debt / Future Work
1. **True engine-native abstraction layer**: Replace shim `pthread.h` with proper `ttak_mutex_t`, `ttak_cond_t` types throughout the codebase.
2. **Cooperative scheduler**: Implement a single-loop task scheduler instead of stubbing `pthread_create`.
3. **64-bit atomic optimization**: Replace global-lock fallback with inline `LDREX`/`STREX` for 32-bit halves or Cortex-M7-specific `__dmb`/`__dsb` barriers where safe.
4. **Memory pool tuning**: Profile actual usage and tune `TTAK_EMBEDDED_POOL_ORDER`, `TTAK_POCKET_POOL_SIZE`, etc. for real workloads.
5. **SDRAM support**: Map large regions to external 8MB SDRAM on STM32F769I-DISC1 if needed.
6. **Newlib `_sbrk`**: Currently unused because `malloc` is fully replaced, but verify no accidental `printf` heap usage.
