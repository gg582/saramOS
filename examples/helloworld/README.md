# Hello World for STM32F769I-DISC1

Bare-metal Hello World for saramOS on STM32F769I-DISC1.

## Build

```bash
make
```

## Flash

Connect the board via USB (ST-Link) and run:

```bash
make flash
```

Or from the project root:

```bash
make flash
```

## Serial Output

Open a serial terminal on the ST-Link VCP (typically `/dev/ttyACM0`):

```bash
picocom /dev/ttyACM0 -b 115200
```

You should see:

```
=== saramOS on STM32F769I-DISC1 ===
Type 'help' for available commands.

saramOS: resilient kernel core init OK
saramOS: arena init OK
saramOS: owner init OK
libttak: async scheduler init OK
example: calculator programs loaded (arith, modulo)
===================================
saramOS> 
```

## Programmable Calculator

The shell includes a tiny Casio-style accumulator program mode.

Built-in examples:

```text
program run arith
program run modulo
```

Create and run a program:

```text
program mycalc
prog> set 10
prog> add 7
prog> mod 5
prog> print
prog> end
program run mycalc
```

Supported instructions: `set`, `add`, `sub`, `mul`, `div`, `mod`, `print`.
