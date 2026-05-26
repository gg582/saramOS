# Hello World RTOS Profile (ESP32-WROOM32)

This example focuses exclusively on ESP32-WROOM32 bring-up. It shows how to **reuse libTTAK primitives** (arenas, epoch GC, async workers, TTL-guarded I/O, stats) and only add the thin ESP32-specific HAL glue required for UART, SysTick, and PendSV-style scheduling. The end-to-end footprint stays in the 2–5 KLoC range because the deterministic lifetime/scheduler infrastructure already lives inside libTTAK.

## What libTTAK already gives us

| RTOS concern | Existing module | Notes |
| --- | --- | --- |
| Deterministic heap | `ttak/mem/mem.h`, `ttak/mem/epoch_gc.h` | Arena + epoch reclamation already isolate lifetimes, so the embedded port only needs a thin `ttak_buddy_alloc()` backend (~200 LOC) backed by SRAM/PSRAM pools. |
| Task management | `ttak/thread/pool.h`, `ttak/async/task.h` | Latin-square sharded queues and worker contexts are ready; in bare metal just map each shard to a cooperative fiber or one Cortex-M free-running thread (~700 LOC glue). |
| Timing & ticks | `ttak/timing/timing.h` | Replace `clock_gettime` with TIM2/DWT-backed monotonic counter (~100 LOC). |
| Driver lifetimes | `ttak/io/io.h` | TTL-guarded descriptors already model UART/SPI handles; embed HAL handles inside `ttak_io_guard_t`. |
| Telemetry | `ttak/stats/system_usage.h` | RSS/CPU queries translate cleanly to Cortex-M: expose stack-high watermark + allocator usage (~120 LOC). |
| Configuration | `ttak/limit/`, `ttak/log/` | Existing limiters/loggers become the board control plane with only format tweaks.

Because these capabilities exist, the RTOS shell mainly wires three new layers:

1. **Low-level board support (BSP):** clock tree, UART, timer, interrupt matrix. Estimated 700–900 LOC using ESP-IDF HALs.
2. **Scheduler shim:** binding libTTAK’s async worker to the ESP32 pendable scheduler interrupt + cycle counter. Estimated 600–800 LOC.
3. **Peripheral facades:** UART0 console, SPI flash, Wi-Fi coexistence bridge, cooperative shell. Estimated 400–800 LOC.

That keeps the total well within the requested 2–5 KLoC and still leverages libTTAK’s deterministic lifetimes and lock-free queues.

## ESP32-WROOM32 port sketch

1. **Memory Pools:** Reserve two regions (fast SRAM, PSRAM) and expose them via `ttak_mem_install_pool(struct ttak_mem_pool_desc *desc, size_t count)`. Each pool uses libTTAK’s buddy tier (`TTAK_ALLOC_TIER_BUDDY`) so ordinary code keeps calling `ttak_mem_alloc_safe()` without noticing the backend change.
2. **Scheduler Bridge:** Install a `ttak_worker_t` per Cortex-M hardware thread (usually 1) and run it from the PendSV handler. Queue slices use the existing priority queue but use SysTick (1 ms) as the quiescence boundary.
3. **Interrupt Discipline:** ISR stubs push work into `ttak_task_t` objects that are already lock-free; the ESP32-specific logic is the IRQ-to-task trampoline bound to the pendable scheduler interrupt.
4. **UART Console:** Wrap ESP32’s UART HAL init/send calls behind a struct that implements `rtos_serial_write()` so desktop builds print to stdout while the target writes to UART0/UART1.
5. **RSS Telemetry Substitute:** On the target expose arena high-water marks (`ttak_mem_tree_stats`) and stack usage. The desktop build keeps using `/proc/self`. The app code does not change.

## Demo application

The `hello_rtos.c` demo registers a mock UART console, prints `hello, world`, allocates a navigation grid via `malloc`, runs a single BFS to route from `S` to `E`, renders the path as ASCII art, frees every buffer, and finally queries the libTTAK RSS helper. On real silicon the same code runs inside the RTOS shell with the UART backed by HAL, while the desktop build uses stdout.

### Build and run

From the root directory:
```bash
make app APP_DIR=examples/helloworld          # cross-builds helloworld
make deploy-ocd APP_DIR=examples/helloworld   # cross-build + flash with U-Boot & OpenOCD
```

Or from the example directory:
```bash
make             # cross-builds build/<board>/hello_rtos.bin using boards.config
make native-run  # builds/runs the host harness if you need a quick regression check
```

`make` no longer produces a runnable x86 binary; it reads `<repo>/boards.config` (currently `esp32-wroom32`) and generates the ESP32 artefact plus flashing hooks. The default `tools/esp32_flash.sh` helper mirrors the ESP-IDF esptool workflow and will flash `hello_rtos.bin` using `BOARD_FLASH_PORT`, `BOARD_FLASH_BAUD`, and `BOARD_FLASH_OFFSET` from that config. Override `ESP32_FLASH_TOOL` or `ESPTOOL_PY` if you need a custom/flashing command. Use `make native-run` explicitly when you want to exercise the stdout version on your dev box.

To keep the RTOS shell strictly POSIX-compatible, the example Makefile trims the libTTAK build to the `LIBTTAK_POSIX_SRC_DIRS` module list. Only the worker/memory/timing primitives that the demo actually touches are compiled for the board profile. Extend that variable if your application needs additional subsystems.

### Expected log

```
[uart0] boot tick=123456 ns
[uart0] hello, world (libttak RTOS profile)
[uart0] running BFS over 16x9 grid...
[uart0]
################
#S***#........#
#.#*#*.#####..#
#..#*#*****#..#
##.#*###*#*#E#
#..*****...#..#
###############

[uart0] BFS steps: 31
[uart0] heap released, RSS snapshot: 1.23 MB
```

(The exact tick/RSS values will differ per machine.)

This flow demonstrates that the same deterministic code path can serve as the first RTOS workload on ESP32-WROOM32 once the thin BSP + scheduler glue is added.

## ESP32 HAL UART + scheduler bridge

`rtos_port.c` now contains the integration points for ESP32-WROOM32 and uses the shared HAL header `include/hal/esp32_wroom32.h`:

1. **UART swap:** building with `-DTTAK_TARGET_ESP32` (wired in the Makefile) routes `rtos_uart_*` to `esp_hal_uart_*` functions. Desktop builds still get the printf-backed shim, but no application code changes are required when moving to the UART HAL.
2. **Tick/scheduler hook:** call `rtos_port_scheduler_init(async_pool, 0, 1000);` after creating the libTTAK worker pool. The helper kicks off the esp_timer-based tick and uses `esp_hal_pend_scheduler()` to trigger the PendSV-style cooperative drain. `ttak_worker_run_cooperative` executes inside that ISR so FPU context-switching gets exercised. Host builds provide no-op stubs so the same code compiles everywhere.

The stub implementation in `src/hal/esp32-wroom32/esp32_hal_stub.c` keeps the build working on hosts; replace it with your actual ESP-IDF HAL glue when bringing up hardware. Keep `boards.config` in sync with your UART port/baud, or point `ESP32_FLASH_TOOL` at any alternative script if you need a different flashing flow.
