ROOT_DIR := $(realpath ../..)
CONFIG ?= stm32f769i-disc1
BOARDS_CONFIG := $(ROOT_DIR)/configs/$(CONFIG)

ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error Configuration file $(BOARDS_CONFIG) not found)
endif

include $(BOARDS_CONFIG)

BUILD_DIR := build
BOARD_BUILD_DIR := $(BUILD_DIR)/$(BOARD)

HAL_DIR := $(ROOT_DIR)/src/hal/$(BOARD)
HAL_SRC := $(HAL_DIR)/hal_uart.c $(HAL_DIR)/hal_sys.c
STARTUP := $(HAL_DIR)/startup.S
LD_SCRIPT := $(HAL_DIR)/linker.ld

OS_DIR := $(ROOT_DIR)/src/os
OS_SRC := $(OS_DIR)/saramos_arena.c $(OS_DIR)/saramos_owner.c

LIBTTAK_DIR := $(ROOT_DIR)/engine/libttak
LIBTTAK_A := $(LIBTTAK_DIR)/lib/libttak.a

APP_SRC := main.c game.c syscalls.c $(OS_SRC)

INCLUDES := -I$(ROOT_DIR)/include -I$(HAL_DIR) -I$(LIBTTAK_DIR)/include

CROSS_CC := $(BOARD_CC)
CROSS_OBJCOPY := $(BOARD_OBJCOPY)
CROSS_SIZE := $(BOARD_SIZE)

CFLAGS := $(BOARD_CFLAGS) $(INCLUDES) -std=c17
ASFLAGS := $(BOARD_CFLAGS) $(INCLUDES)
LDFLAGS := $(BOARD_LDFLAGS) -T$(LD_SCRIPT) -L$(LIBTTAK_DIR)/lib -lttak -Wl,-Map=$(BOARD_BUILD_DIR)/hello_rtos.map

BOARD_ELF := $(BOARD_BUILD_DIR)/hello_rtos.elf
BOARD_BIN := $(BOARD_BUILD_DIR)/hello_rtos.bin
BOARD_HEX := $(BOARD_BUILD_DIR)/hello_rtos.hex

.PHONY: all board clean size flash libttak

all: board

libttak:
	$(MAKE) -C $(LIBTTAK_DIR) clean
	$(MAKE) -C $(LIBTTAK_DIR) EMBEDDED_BAREMETAL=1 CC=$(BOARD_CC) AR=$(BOARD_AR) \
		CFLAGS="-Wall -std=c17 -Iinclude -DEMBEDDED=1 -DEMBEDDED_BAREMETAL=1 -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -Os -ffunction-sections -fdata-sections -fno-lto -D_Thread_local=" \
		LDFLAGS=""

board: $(BOARD_BIN) $(BOARD_HEX) size

$(BOARD_BUILD_DIR):
	mkdir -p $(BOARD_BUILD_DIR)

$(LIBTTAK_A): libttak

$(BOARD_ELF): $(BOARD_BUILD_DIR) $(APP_SRC) $(HAL_SRC) $(STARTUP) $(LD_SCRIPT) $(LIBTTAK_A)
	$(CROSS_CC) $(CFLAGS) $(ASFLAGS) $(APP_SRC) $(HAL_SRC) $(STARTUP) $(LDFLAGS) -o $@

$(BOARD_BIN): $(BOARD_ELF)
	$(CROSS_OBJCOPY) -O binary $< $@

$(BOARD_HEX): $(BOARD_ELF)
	$(CROSS_OBJCOPY) -O ihex $< $@

size: $(BOARD_ELF)
	$(CROSS_SIZE) $<

clean:
	rm -rf $(BUILD_DIR)

flash: $(BOARD_BIN)
	$(ROOT_DIR)/tools/stm32_flash.sh $(BOARD_BIN)
