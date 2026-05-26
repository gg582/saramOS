#!/usr/bin/env bash
# Flash helper using OpenOCD for ESP32.
# This script flashes both U-Boot (bootloader) and saramOS (application).

set -euo pipefail

UBOOT_BIN=${1:-"third_party/u-boot/u-boot.bin"}
APP_BIN=${2:-""}

if [[ -z "$APP_BIN" ]]; then
  echo "[openocd-flash] usage: esp32_openocd_flash.sh <uboot_bin> <app_bin>" >&2
  exit 1
fi

OPENOCD_INTERFACE=${OPENOCD_INTERFACE:-"interface/ftdi/esp32_devkitj_v1.cfg"}
OPENOCD_TARGET=${OPENOCD_TARGET:-"target/esp32.cfg"}

# Default offsets (example)
UBOOT_OFFSET=${UBOOT_OFFSET:-0x1000}
APP_OFFSET=${APP_OFFSET:-0x10000}

echo "[openocd-flash] Flashing U-Boot ($UBOOT_BIN) at $UBOOT_OFFSET"
echo "[openocd-flash] Flashing saramOS ($APP_BIN) at $APP_OFFSET"

openocd -f "$OPENOCD_INTERFACE" -f "$OPENOCD_TARGET" \
  -c "init; halt; esp32 flash_bank_virt 0" \
  -c "program $UBOOT_BIN verify $UBOOT_OFFSET" \
  -c "program $APP_BIN verify $APP_OFFSET" \
  -c "reset run; shutdown"

echo "[openocd-flash] Done."
