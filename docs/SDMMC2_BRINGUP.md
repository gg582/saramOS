# STM32F769I-DISCO SDMMC2 bring-up

This patch adds a polling-only SDMMC2 low-level driver for the on-board microSD slot and a CLI command:

```text
saramOS> sdtest
```

Expected successful log:

```text
[SDTEST] start
[SD] gpio init
[SD] IDR: ... detect=inserted
[SD] sdmmc2 clock init
[SD] CMD0
[SD] CMD8
[SD] ACMD41
[SD] CMD2
[SD] CMD3
[SD] CMD7 select
[SD] CMD16 block len 512
[SD] init OK, 1-bit mode
[SDTEST] read LBA0
[SDTEST] LBA0 OK: ... sig=AA55
[SDTEST] boot sector/MBR signature OK
```

Pin map used:

| Signal | Pin | AF |
|---|---:|---:|
| SDMMC2_CK | PD6 | AF11 |
| SDMMC2_CMD | PD7 | AF11 |
| SDMMC2_D0 | PG9 | AF11 |
| SDMMC2_D1 | PG10 | AF11 |
| SDMMC2_D2 | PB3 | AF10 |
| SDMMC2_D3 | PB4 | AF10 |
| SD_DETECT | PI15 | GPIO input, active-low |

Bring-up notes:

- Driver intentionally starts in 1-bit mode. This isolates CMD/CLK/D0 before FAT or 4-bit mode.
- Run `sdtest` before mounting FAT. FAT must not call disk read until LBA0 succeeds.
- PB3 is shared with debug/JTAG/SWO-related functions on many STM32 boards. Disable SWO/JTAG usage if D2/D3/4-bit mode is later enabled.
- If it fails at `detect=empty`, check PI15/card-detect polarity or socket switch.
- If it fails at CMD8/ACMD41, check SDMMC2 clock enable, PD6/PD7 AF, and pull-ups.
- If it fails at read LBA0, check PG9 D0 first.
