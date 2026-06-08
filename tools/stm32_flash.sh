#!/bin/bash
set -euo pipefail

BINFILE="${1:-build/stm32f769i-disc1/hello_rtos.bin}"
OPENOCD_CFG="${OPENOCD_CFG:-board/stm32f769i-disco.cfg}"
FLASH_ADDR="${FLASH_ADDR:-0x08000000}"

if [ ! -f "$BINFILE" ]; then
    echo "Error: Binary file not found: $BINFILE"
    echo "Usage: $0 <path-to-binary>"
    exit 1
fi

echo "[flash] Programming STM32F769I-DISC1 via ST-Link/OpenOCD..."
echo "[flash] Binary: $BINFILE"
echo "[flash] Address: $FLASH_ADDR"

openocd -f "$OPENOCD_CFG" \
    -c "init" \
    -c "reset init" \
    -c "halt" \
    -c "flash probe 0" \
    -c "flash write_image erase $BINFILE $FLASH_ADDR" \
    -c "verify_image $BINFILE $FLASH_ADDR" \
    -c "reset halt" \
    -c "reset run" \
    -c "shutdown"

echo "[flash] Done."
