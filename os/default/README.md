# saramOS Default Image

This is the default saramOS image for STM32F769I-DISC1. It provides a common
shell, the resilient RTOS kernel core, lwIP networking, FatFs/SD card support,
and a programmable calculator mode.

Applications in `apps/` can be linked on top of this image by supplying
additional source files via `EXTRA_APP_SRCS`.

## Build the base image only

```bash
make -C os/default
# or from the project root
make APP_DIR=os/default
```

The firmware is written to:

```
build/stm32f769i-disc1/saramos.bin
```

## Flash the base image

```bash
make -C os/default flash
# or
make APP_DIR=os/default flash
```

## Optional built-in calculator programs

Built-in example programs (`arith`, `modulo`) are enabled by default.
To build without them:

```bash
make -C os/default ENABLE_BUILTIN_EXAMPLES=0
```

## Optional applications

Applications in `apps/` can be linked into the OS image. For example:

```bash
cd apps/example/game/sudoku
make flash
```

Or from the project root:

```bash
make                  # default app is sudoku
make APP_DIR=<path>   # build any app directory
```
