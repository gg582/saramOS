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
Hello World
===================================
Heartbeat from saramOS
Heartbeat from saramOS
...
```
