# Sudoku Example App

This is the representative example application for saramOS. It links the
mini 4x4 Sudoku game on top of the `os/default` image and registers a `sudoku`
CLI command via `app_register_commands()`.

## Build

From the project root (default app):

```bash
make
```

Or directly from this directory:

```bash
cd apps/example/game/sudoku
make
```

The firmware image is written to:

```
build/stm32f769i-disc1/saramos.bin
```

## Flash

```bash
make flash
```

## Usage

Open a serial terminal (115200 8-N-1) and type:

```text
sudoku
```

Enter moves as `row col value`, for example `1 2 3`. Use `0` to clear a cell,
`r` to reset, `h` for help, and `q` to quit.

## How it works

`sudoku.c` implements the game logic with platform-independent `sudoku_puts_fn`
and `sudoku_getc_fn` callbacks. `app_sudoku.c` wires those callbacks to the
board UART and registers the `sudoku` command with the shell in `os/default`.
