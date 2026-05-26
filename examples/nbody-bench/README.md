# N-Body RTOS Benchmark Example

This example packages the `nbody_bench.c` benchmark into a runnable host binary so you can validate your RTOS build tooling separately from the kernel. The code stays malloc-free, divides work across logical tasks, and prints the 16 required performance metrics once the simulation completes.

## Build targets

From the root directory:
```bash
make app APP_DIR=examples/nbody-bench          # cross-builds nbody-bench
make deploy-ocd APP_DIR=examples/nbody-bench   # cross-build + flash with U-Boot & OpenOCD
```

Or from the example directory:
```bash
make             # cross-builds build/<board>/nbody_bench.bin using boards.config
make native-run  # builds/runs the pthread harness on the host for quick sanity checks
```

The default board is read from `<repo>/boards.config` (currently `esp32-wroom32`). Override `BOARD` or `BOARD_TOOLCHAIN_PREFIX` in that config if you need a different ESP32 variant/toolchain. The shared `tools/esp32_flash.sh` helper will locate `esptool.py` from your ESP-IDF installation and respects `BOARD_FLASH_PORT`, `BOARD_FLASH_BAUD`, and `BOARD_FLASH_OFFSET`. Override `ESP32_FLASH_TOOL` if you want a different flashing command. `make native-run` is the only target that produces a host executable; the default build now mirrors the embedded workflow.

## Porting notes

* Replace `main.c`'s pthread harness with your RTOS task creation API and wire the provided hooks (`nbody_mark_task_ready`, `nbody_record_*`) into your scheduler/ISR instrumentation. The shared ESP32 HAL (`include/hal/esp32_wroom32.h`, `src/hal/esp32-wroom32/esp32_hal_stub.c`) already routes the benchmark's console output through the board UART.
* The benchmark itself lives in `nbody_bench.c/.h`; you can compile those two files directly into your RTOS firmware to avoid linking the full libTTAK archive.
* Set `NBODY_DEMO_BODIES`, `NBODY_MAX_TASKS`, or other `#define`s at compile time to scale the workload for your board.
