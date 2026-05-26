# Root Makefile for saramOS

# Default configuration is esp32
CONFIG ?= esp32
BOARDS_CONFIG := configs/$(CONFIG)

ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error Configuration file $(BOARDS_CONFIG) not found)
endif

include $(BOARDS_CONFIG)

UBOOT_DIR := third_party/u-boot
UBOOT_BIN := $(UBOOT_DIR)/u-boot.bin

# Default APP_DIR if not provided.
APP_DIR ?= examples/helloworld

.PHONY: all u-boot app clean qemu-run

all: u-boot app

$(UBOOT_BIN):
	@if [ "$(CONFIG)" = "esp32" ]; then \
		echo "[uboot] Building U-Boot for Xtensa..."; \
		$(MAKE) -C $(UBOOT_DIR) CROSS_COMPILE=$(BOARD_TOOLCHAIN_PREFIX) esp32_defconfig; \
		$(MAKE) -C $(UBOOT_DIR) CROSS_COMPILE=$(BOARD_TOOLCHAIN_PREFIX) -j$(shell nproc); \
		echo "[uboot] Converting to ESP32 image..."; \
		esptool --chip esp32 elf2image $(UBOOT_DIR)/u-boot -o $(UBOOT_BIN); \
	else \
		echo "[uboot] U-Boot build skipped for $(CONFIG)"; \
	fi

u-boot: $(UBOOT_BIN)

app:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) board

qemu-run: app
	@if [ "$(CONFIG)" = "x64-qemu" ]; then \
		echo "[qemu] Starting AMD64 VM (POSIX Runner)..."; \
		$(APP_DIR)/build/x64-qemu/hello_rtos; \
	else \
		echo "Error: qemu-run is only supported for x64-qemu configuration"; \
		exit 1; \
	fi

clean:
	-$(MAKE) -C $(UBOOT_DIR) clean
	-$(MAKE) -C $(APP_DIR) clean
