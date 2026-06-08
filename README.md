# saramOS

**saramOS** is a POSIX-free bare-metal runtime/RTOS project targeting STM32F769I-DISC1 (Cortex-M7). It ports and integrates [libttak](engine/libttak) as its core engine, directly referencing and wrapping libttak's **generational arena** and **ownership** concepts at the OS level.

---

## Table of Contents

1. [Hardware Requirements](#hardware-requirements)
2. [Software Requirements](#software-requirements)
3. [Project Structure](#project-structure)
4. [How to Build](#how-to-build)
5. [How to Flash](#how-to-flash)
6. [libttak Integration: Arena & Ownership](#libttak-integration-arena--ownership)
7. [Debugging & Tips](#debugging--tips)
8. [License](#license)

---

## Hardware Requirements

| Item | Specification |
|------|------|
| Board | **STM32F769I-DISC1** (STM32F769NIH6) |
| Core | ARM Cortex-M7, 216 MHz (current example boots with 16 MHz HSI) |
| Flash | 2 MB (0x0800_0000) |
| SRAM | 512 KB (0x2000_0000) |
| Debugger | Embedded ST-Link/V2-1 (Micro-USB CN14) |
| UART | USART1 PA9(TX) / PA10(RX), 115200-8-N-1 (ST-Link Virtual COM Port) |

---

## Software Requirements

- **GNU Arm Embedded Toolchain** (`arm-none-eabi-gcc`, `arm-none-eabi-ar`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`)
- **OpenOCD** (for flashing and debugging)
- **GNU Make**
- (Optional) `screen`, `minicom`, `picocom`, etc., to monitor UART logs

### Installation Example on Ubuntu/Debian

```bash
sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi openocd make
# UART monitoring
sudo apt install picocom
picocom -b 115200 /dev/ttyACM0
```

### macOS (Homebrew)

```bash
brew install --cask gcc-arm-embedded
brew install openocd make
```

---

## Project Structure

```
saramOS/
├── Makefile                  # Top-level orchestration (specifies APP_DIR, CONFIG)
├── README.md                 # This file (English)
├── README.ko.md              # Korean version of README
├── ROADMAP.md                # Roadmap for libttak POSIX-free refactoring
├── configs/
│   └── stm32f769i-disc1      # Board-specific CFLAG/LDFLAG definitions
├── engine/
│   └── libttak/              # libttak submodule (bare-metal branch)
│       ├── include/          # Public headers (ttak/mem/arena_helper.h, etc.)
│       ├── src/              # Sources (baremetal_alloc.c, baremetal_pthread.c, etc.)
│       ├── Makefile          # libttak standalone build
│       └── lib/libttak.a     # Build output
├── examples/
│   └── helloworld/
│       ├── Makefile          # Actual build rules
│       ├── main.c            # Application entry point
│       ├── syscalls.c        # Newlib stub (_sbrk, _write → UART)
│       └── build/stm32f769i-disc1/
│           ├── hello_rtos.elf
│           ├── hello_rtos.bin   # Target binary for flashing
│           └── hello_rtos.hex
├── include/
│   ├── hal/
│   │   └── stm32f769i-disc1.h   # Register definition HAL
│   └── os/
│       ├── saramos_arena.h   # saramOS arena wrapper header
│       └── saramos_owner.h   # saramOS owner wrapper header
├── src/
│   ├── hal/stm32f769i-disc1/
│   │   ├── hal_sys.c         # Cache/Clock/FLASH latency initialization
│   │   ├── hal_uart.c        # USART1 driver
│   │   ├── linker.ld         # Linker script
│   │   └── startup.S         # Vector table + Reset_Handler
│   └── os/
│       ├── saramos_arena.c   # libttak arena_helper wrapper
│       └── saramos_owner.c   # libttak owner wrapper
├── tools/
│   └── stm32_flash.sh        # OpenOCD flashing script
└── third_party/
    ├── newlib_posix/
    └── u-boot/
```

---

## How to Build

### 1) Full Build (libttak + Application)

At the project root:

```bash
make
```

This command performs the following in order:

1. Cross-compiles `engine/libttak` with `EMBEDDED_BAREMETAL=1` → generates `libttak.a`
2. Compiles and links `examples/helloworld` (`main.c`, `syscalls.c`, HAL source, and `src/os/` wrappers)
3. Converts `.elf` → `.bin` / `.hex` (`objcopy`)
4. Prints section sizes using `arm-none-eabi-size`

Example output:

```
   text    data     bss     dec     hex filename
  25261     292  178352  203905   31c81 build/stm32f769i-disc1/hello_rtos.elf
```

- **text**: Code and RO data written to flash (~25 KB)
- **data**: Initialized RW data
- **bss**: Zero-initialized static pool (includes libttak buddy/pocket/VMA/heap, ~178 KB)

### 2) Individual Build Steps

```bash
# Rebuild libttak only
make -C examples/helloworld libttak

# Build application only (when libttak already exists)
make -C examples/helloworld board

# Check map file / section sizes
make size

# Clean up
make clean
```

### 3) Troubleshooting Build Failures

- Verify that `arm-none-eabi-gcc --version` executes successfully.
- Ensure the `engine/libttak` submodule is initialized (`git submodule update --init --recursive`).
- OpenOCD is only required for flashing, **not for the build itself**.

---

## How to Flash

### Method A: `make flash` (Recommended)

After a successful build:

```bash
make flash
```

This command invokes `tools/stm32_flash.sh` to flash the binary using OpenOCD.

Internal operation:

```bash
openocd -f board/stm32f769i-disco.cfg \
    -c "init" \
    -c "reset init" \
    -c "halt" \
    -c "flash probe 0" \
    -c "flash write_image erase build/stm32f769i-disc1/hello_rtos.bin 0x08000000" \
    -c "verify_image build/stm32f769i-disc1/hello_rtos.bin 0x08000000" \
    -c "reset halt" \
    -c "reset run" \
    -c "shutdown"
```

> You must connect the board's **CN14 (USB ST-LINK)** port to your PC.

### Method B: Manual OpenOCD

If `make flash` fails or you want to flash a different binary:

```bash
# 1) Start OpenOCD server in one terminal (optional, for debugging)
openocd -f board/stm32f769i-disco.cfg

# 2) Or flash in a single command
cd examples/helloworld
openocd -f board/stm32f769i-disco.cfg \
    -c "init; reset init; halt; flash probe 0" \
    -c "flash write_image erase build/stm32f769i-disc1/hello_rtos.bin 0x08000000" \
    -c "verify_image build/stm32f769i-disc1/hello_rtos.bin 0x08000000" \
    -c "reset halt; reset run; shutdown"
```

### Method C: ST-Link Utility (Alternative)

If you have the `st-link` CLI tool installed:

```bash
st-flash --reset write build/stm32f769i-disc1/hello_rtos.bin 0x08000000
```

Alternatively, you can use the **STM32CubeProgrammer** GUI on Windows/Mac:
- Interface: ST-Link
- Address: `0x08000000`
- File: `examples/helloworld/build/stm32f769i-disc1/hello_rtos.bin`

### Method D: GDB + OpenOCD (Debugging & Flashing)

```bash
# Terminal 1
openocd -f board/stm32f769i-disco.cfg

# Terminal 2
arm-none-eabi-gdb build/stm32f769i-disc1/hello_rtos.elf
(gdb) target extended-remote localhost:3333
(gdb) load          # Load to flash
(gdb) monitor reset halt
(gdb) continue
```

### Checking UART Logs after Flashing

```bash
# Linux
picocom -b 115200 /dev/ttyACM0

# macOS
picocom -b 115200 /dev/tty.usbmodemXXXX

# Or using screen
screen /dev/ttyACM0 115200
```

Expected output on successful boot:

```
=== saramOS on STM32F769I-DISC1 ===
Type 'help' for available commands.

saramOS: arena init OK
saramOS: owner init OK
libttak: async scheduler init OK
===================================
saramOS> 
```

---

## libttak Integration: Arena & Ownership

saramOS uses a bare-metal ported version of libttak as its engine. Instead of simply linking the library, **saramOS's own memory and resource management layers directly reference and wrap libttak's arena and owner concepts**.

### Arena (Generational Memory Management)

libttak's `ttak_arena_env_t` / `ttak_arena_generation_t` provides **epoch-based generational allocation**:

- Fast bump-pointer allocation within a generation.
- Reusing the entire generation at once using `reset` (in-place discard).
- Retiring a generation transfers it to the epoch GC for safe reclamation later.

**saramOS Wrapper**: `saramos_arena_t` ([saramos_arena.h](file:///home/yjlee/saramOS/include/os/saramos_arena.h), [saramos_arena.c](file:///home/yjlee/saramOS/src/os/saramos_arena.c))

```c
#include <os/saramos_arena.h>

saramos_arena_t arena;
saramos_arena_init(&arena);

void *buf = saramos_arena_alloc(&arena, 256);   /* Allocates 256B in current generation */
size_t rem = saramos_arena_remaining(&arena);   /* Checks remaining space */

saramos_arena_reset(&arena);                    /* Reuse all space in current generation */
saramos_arena_rotate(&arena);                   /* Starts a new generation, yielding the old to GC */

saramos_arena_destroy(&arena);
```

Bare-metal defaults:
- Generation size: 4 KB
- Default chunk: 256 B
- Removed `_Thread_local`; all pools are statically allocated in `.bss`.

### Ownership (Resource Isolation)

libttak's `ttak_owner_t` represents a **resource sandbox for each subsystem**:

- Name-based resource registration (`register_resource`).
- Name-based function registration (`register_func`).
- Isolated execution via `execute` which passes registered resources as the context (`ctx`).
- Auto-cleanup of all registered resources and maps on `destroy`.

**saramOS Wrapper**: `saramos_owner_t` ([saramos_owner.h](file:///home/yjlee/saramOS/include/os/saramos_owner.h), [saramos_owner.c](file:///home/yjlee/saramOS/src/os/saramos_owner.c))

```c
#include <os/saramos_owner.h>

saramos_owner_t owner;
saramos_owner_init(&owner, "uart_driver");

/* Register resource */
saramos_owner_register_resource(&owner, "uart_ctx", &uart_instance);

/* Register function */
saramos_owner_register_func(&owner, "init", my_uart_init_func);

/* Execute: Run "init" function with "uart_ctx" passed as ctx */
saramos_owner_execute(&owner, "init", "uart_ctx", NULL);

/* Batch cleanup on subsystem termination */
saramos_owner_destroy(&owner);
```

Through this structure, each driver or task in saramOS can have **its own owner**, preventing memory leaks by allowing resources to be reclaimed all at once.

### Internal Memory Pools (Bare-Metal)

| Pool | Size | Purpose |
|------|------|---------|
| Buddy pool | 32 KB | Mid-to-large block allocation |
| Pocket pool | 32 KB | Small objects (≤512 B) |
| VMA region | 16 KB | Medium-sized mapping |
| Large region | 16 KB | Fallback for large objects |
| Baremetal heap | 64 KB | First-fit heap in `baremetal_alloc.c` |
| Other `.bss` | ~14 KB | Stacks and other static variables |
| **Total** | **~174 KB** | Well within the 512 KB SRAM limit |

---

## Debugging & Tips

### Checking the Linker Map

You can inspect symbol addresses and section layouts in `examples/helloworld/build/stm32f769i-disc1/hello_rtos.map`.

### In Case of HardFault

1. Connect via OpenOCD:
   ```bash
   openocd -f board/stm32f769i-disco.cfg
   ```
2. Inspect `info registers` and `backtrace` in GDB.
3. Check stack/heap boundaries in `linker.ld`.

### If UART Output is Missing

- Confirm the connection to the board's **CN14 (USB ST-LINK)**.
- Terminal emulator settings: **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control**.
- Verify that `hal_uart_init()` is called immediately at the beginning of `main()`.

### Optimizing Memory Usage

You can adjust the following constants in `engine/libttak/internal/ttak/mem_internal.h`:

```c
#define TTAK_EMBEDDED_POOL_ORDER 15   /* 2^15 = 32 KB buddy */
#define TTAK_POCKET_POOL_SIZE    (4096 * 8)  /* 32 KB */
#define TTAK_VMA_REGION_SIZE     (16 * 1024) /* 16 KB */
#define TTAK_LARGE_REGION_SIZE   (16 * 1024) /* 16 KB */
```

> Decreasing these values reduces `.bss` size but increases the likelihood of runtime allocation failures.

### Applying libttak Modifications

After modifying libttak source code, run `make clean` or `make -C examples/helloworld libttak` to rebuild the static library before building the main application.

---

## License

- **saramOS**: Refer to the top-level `LICENSE` file.
- **libttak**: Refer to `engine/libttak/LICENSE`.

---

*Happy hacking on bare-metal!*
