# Third-party components

This directory contains external libraries integrated into saramOS.

## lwIP 2.2.0

- Source: `git clone --depth 1 --branch STABLE-2_2_0_RELEASE https://github.com/lwip-tcpip/lwip.git`
- License: BSD-style (see `lwip/COPYING`)
- Used for: TCP/IP stack, DHCP client, and HTTP server (`contrib/apps/httpserver`).
- Integration: bare-metal, NO_SYS=1, polled/IRQ Ethernet driver in `src/hal/stm32f769i-disc1/hal_eth.c`.

## FatFs R0.15

- Source: http://elm-chan.org/fsw/ff/arc/ff15.zip
- License: FatFs license (see `fatfs/LICENSE.txt`)
- Used for: SD card filesystem layer on top of `hal_sdmmc` block driver.
- Integration: `os/default/sd_diskio.c` implements the FatFs disk I/O interface.

## u-boot

- Already present in the repository as a reference source.
- Used as a register-level reference for STM32F7 Ethernet and SDMMC behavior, not linked directly.

## newlib_posix

- Already present in the repository.
- Provides POSIX stubs and Newlib integration for the bare-metal build.
