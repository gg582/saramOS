# saramOS libttak POSIX-Free Bare-Metal Refactoring ROADMAP

## Objective
Transform `engine/libttak` into a fully POSIX-free, bare-metal capable engine for STM32F769I-DISC1 (Cortex-M7). No pthread, no dynamic malloc/free, no generic OS libc dependencies.

---

## Milestone 1: Isolation of Standard Libraries & Abstracting Synchronizations
**Goal:** Eliminate all direct libc dynamic allocation and POSIX threading primitives.

- [ ] Audit every `malloc`, `calloc`, `realloc`, `free` call in `src/` and `internal/`.
- [ ] Redirect all dynamic allocation paths to `ttak_mem_pocket` static pools.
- [ ] Define opaque engine-native synchronization types:
  - `ttak_mutex_t`
  - `ttak_cond_t`
  - `ttak_thread_t`
- [ ] Replace all `pthread_mutex_t` references with `ttak_mutex_t` macros/abstraction layer.
- [ ] Replace all `pthread_cond_t` references with `ttak_cond_t` macros/abstraction layer.
- [ ] Guard POSIX implementations under `#if !defined(EMBEDDED)` or `#if defined(TTAK_HOSTED)`.

---

## Milestone 2: POSIX-Free Core Worker & Threading Model Refactoring
**Goal:** Remove `pthread_create`, `pthread_join`, and any multi-threaded worker assumptions from bare-metal builds.

- [ ] Strip `pthread_create` and `pthread_join` from `src/thread/worker.c`.
- [ ] Implement single cooperative worker loop for `EMBEDDED` builds (no preemption, no threads).
- [ ] Implement bare-metal synchronization primitives using interrupt disabling:
  - `__disable_irq()` / `__enable_irq()` on Cortex-M
  - Inline assembly atomic operations (`LDREX`/`STREX`) where needed
- [ ] Ensure `#if defined(EMBEDDED)` blocks contain zero POSIX headers.

---

## Milestone 3: Embedded Optimization for Memory Pocket & VMA
**Goal:** Fix memory layout so all allocations live in static compile-time pools mapped to STM32 SRAM.

- [ ] Hard-code `TTAK_VMA_REGION_SIZE` and mapping offsets to STM32F769I internal SRAM/SDRAM regions in `internal/ttak/mem_internal.h` using `#ifndef` guards.
- [ ] Convert pocket page pool from any dynamic mapping to a compile-time static array placed in a specific BSS/memory section.
- [ ] Ensure `ttak_mem_alloc` / `ttak_mem_free` operate only on static pools (no heap walk).
- [ ] Validate that no allocation path exceeds 512KB SRAM limit.

---

## Milestone 4: saramOS Integration & Target Flashing
**Goal:** Produce a flashable binary where the engine initializes, allocates, and loops without HardFault.

- [ ] Update `examples/helloworld/Makefile` to link `libttak_stm32f769i.a`.
- [ ] Ensure CFLAGS include `-nostdlib`, `-ffreestanding`, `-mcpu=cortex-m7`, `-mthumb`.
- [ ] Implement `_sbrk` (returns static pool bounds only) or remove it entirely.
- [ ] Build final ELF/BIN with zero undefined POSIX symbols.
- [ ] Flash onto STM32F769I-DISC1 via OpenOCD.
- [ ] Verify via UART:
  - Engine init success log
  - Allocation survival log (static pool only)
  - System heartbeat loop without HardFault

---

## Immediate Next Action
Run full grep audit on `engine/libttak` for `pthread` and `malloc`/`free`/`calloc`/`realloc` references.
