#!/usr/bin/env bash
# Flash helper that mirrors the default ESP-IDF esptool.py workflow.
# The script expects a raw application binary (e.g., hello_rtos.bin) and
# pushes it to the board at the configured offset.
set -euo pipefail

BIN_PATH=${1:-}
if [[ -z "$BIN_PATH" ]]; then
  echo "[flash] usage: esp32_flash.sh <binary>" >&2
  exit 1
fi

if [[ ! -f "$BIN_PATH" ]]; then
  echo "[flash] binary not found: $BIN_PATH" >&2
  exit 1
fi

resolve_esptool() {
  local candidate
  for candidate in "$@"; do
    if [[ -n "$candidate" && -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

declare -a CANDIDATES=()
CANDIDATES+=("${ESPTOOL_PY:-}")
if command -v esptool.py >/dev/null 2>&1; then
  CANDIDATES+=("$(command -v esptool.py)")
fi
if [[ -n "${IDF_PYTHON_ENV_PATH:-}" ]]; then
  CANDIDATES+=("$IDF_PYTHON_ENV_PATH/bin/esptool.py")
fi
CANDIDATES+=("$HOME/.espressif/tools/python/v6.0/venv/bin/esptool.py")
CANDIDATES+=("$HOME/.espressif/v6.0/esp-idf/components/esptool_py/esptool/esptool.py")

ESPTOOL_BIN=$(resolve_esptool "${CANDIDATES[@]}") || {
  cat >&2 <<'MSG'
[flash] esptool.py not found.
        Install ESP-IDF tools or set ESPTOOL_PY=/path/to/esptool.py.
MSG
  exit 1
}

FLASH_PORT=${ESP32_PORT:-${ESP32_FLASH_PORT:-${BOARD_FLASH_PORT:-/dev/ttyUSB0}}}
FLASH_BAUD=${ESP32_BAUD:-${ESP32_FLASH_BAUD:-${BOARD_FLASH_BAUD:-460800}}}
FLASH_OFFSET=${ESP32_FLASH_OFFSET:-${BOARD_FLASH_OFFSET:-0x10000}}
FLASH_MODE=${ESP32_FLASH_MODE:-dio}
FLASH_FREQ=${ESP32_FLASH_FREQ:-40m}
FLASH_SIZE=${ESP32_FLASH_SIZE:-4MB}
FLASH_CHIP=${ESP32_CHIP:-esp32}
RESET_BEFORE=${ESP32_RESET_BEFORE:-default_reset}
RESET_AFTER=${ESP32_RESET_AFTER:-hard_reset}

if [[ ! -e "$FLASH_PORT" ]]; then
  echo "[flash] warning: serial port $FLASH_PORT does not exist; continuing anyway" >&2
fi

echo "[flash] Programming $BIN_PATH to $FLASH_CHIP via $FLASH_PORT @ ${FLASH_BAUD}bps (offset $FLASH_OFFSET)"

"$ESPTOOL_BIN" \
  --chip "$FLASH_CHIP" \
  --port "$FLASH_PORT" \
  --baud "$FLASH_BAUD" \
  --before "$RESET_BEFORE" \
  --after "$RESET_AFTER" \
  write_flash -z \
  --flash_mode "$FLASH_MODE" \
  --flash_freq "$FLASH_FREQ" \
  --flash_size "$FLASH_SIZE" \
  "$FLASH_OFFSET" "$BIN_PATH"

echo "[flash] Done."
