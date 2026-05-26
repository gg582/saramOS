ROOT_DIR := $(realpath ../..)
CONFIG ?= esp32
BOARDS_CONFIG ?= $(ROOT_DIR)/configs/$(CONFIG)

ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error Configuration file $(BOARDS_CONFIG) not found at $(BOARDS_CONFIG))
endif
include $(BOARDS_CONFIG)

BUILD_DIR ?= build
BOARD_BUILD_DIR := $(BUILD_DIR)/$(BOARD)

# Configuration specific sources and flags
ifeq ($(CONFIG),esp32)
    HAL_SRC := $(ROOT_DIR)/src/hal/esp32-wroom32/esp32_hal_stub.c \
               $(ROOT_DIR)/src/hal/esp32-wroom32/pthread_stub.c
    BOOT_SRC := $(ROOT_DIR)/tools/boot/start.S
    LD_SCRIPT := compat/esp32_custom.ld
    HAL_FIX_H := compat/hal_fix.h
    TTAK_TARGET_FLAGS := -DTTAK_TARGET_ESP32
    LDFLAGS_EXTRA := -Wl,-m,elf32xtensa -T$(LD_SCRIPT) -nostartfiles
else ifeq ($(CONFIG),x64-qemu)
    HAL_SRC := $(ROOT_DIR)/src/hal/x64-qemu/x64_hal.c
    BOOT_SRC := 
    LD_SCRIPT := 
    HAL_FIX_H := compat/x64_fix.h
    TTAK_TARGET_FLAGS := -DTTAK_TARGET_POSIX -D_GNU_SOURCE
    LDFLAGS_EXTRA := -pthread
endif

COMPAT_SRC := compat/embedded_stubs.c
SRC_COMMON := hello_rtos.c rtos_port.c worker_bridge.c
BOARD_SRC := $(SRC_COMMON) $(HAL_SRC) $(COMPAT_SRC) $(BOOT_SRC)

LIBTTAK_DIR := $(ROOT_DIR)/engine/libttak
LIBTTAK_BOARD := $(LIBTTAK_DIR)/lib/libttak_$(BOARD).a

POSIX_COMPAT_DIR := $(ROOT_DIR)/third_party/newlib_posix/include
INCLUDES := -I$(LIBTTAK_DIR)/include -I$(ROOT_DIR)/include -I.

BOARD_ELF := $(BOARD_BUILD_DIR)/hello_rtos.elf
BOARD_BIN := $(BOARD_BUILD_DIR)/hello_rtos.bin

# If x64-qemu, the binary is the elf itself (or just named without .bin)
ifeq ($(CONFIG),x64-qemu)
    BOARD_BIN := $(BOARD_BUILD_DIR)/hello_rtos
endif

CROSS_CC := $(BOARD_CC)
CROSS_AR := $(BOARD_AR)
CROSS_OBJCOPY := $(BOARD_OBJCOPY)

# Memory size overrides
CROSS_MEM_FLAGS := -DTTAK_VMA_REGION_SIZE=32768 \
                   -DTTAK_LARGE_REGION_SIZE=65536 \
                   -DTTAK_POCKET_POOL_SIZE=32768

CROSS_COMMON_FLAGS := -Os -fno-lto -ffunction-sections -fdata-sections -Wall -Wextra \
                      $(CROSS_MEM_FLAGS) \
                      -DBOARD_TARGET=\"$(BOARD)\" -DHELLOWORLD_DEPLOY_RUNNER \
                      $(TTAK_TARGET_FLAGS)

ifeq ($(CONFIG),esp32)
    CROSS_COMMON_FLAGS += -mlongcalls -mauto-litpools
    CROSS_APP_INCLUDES := -I$(POSIX_COMPAT_DIR) $(INCLUDES)
else
    CROSS_APP_INCLUDES := $(INCLUDES)
endif

CROSS_CFLAGS := $(CROSS_COMMON_FLAGS) -std=c17 \
                -include $(HAL_FIX_H) \
                $(CROSS_APP_INCLUDES)

CROSS_SFLAGS := $(CROSS_COMMON_FLAGS) $(INCLUDES)

CROSS_LDFLAGS := -Wl,--gc-sections \
                 $(LDFLAGS_EXTRA) \
                 -L$(LIBTTAK_DIR)/lib -lttak_$(BOARD) -lm

.PHONY: all board clean libttak

all: board

board: $(BOARD_BIN)

$(LIBTTAK_BOARD):
	@echo "[libttak] Building for $(BOARD)..."
	$(MAKE) -C $(LIBTTAK_DIR) CC=$(CROSS_CC) AR=$(CROSS_AR) EMBEDDED=1 \
		EXTRA_CFLAGS="$(TTAK_TARGET_FLAGS)"
	cp $(LIBTTAK_DIR)/lib/libttak.a $(LIBTTAK_BOARD)

$(BOARD_BUILD_DIR):
	mkdir -p $(BOARD_BUILD_DIR)

$(BOARD_ELF): $(BOARD_BUILD_DIR) $(LIBTTAK_BOARD) $(BOARD_SRC)
	$(CROSS_CC) $(CROSS_CFLAGS) $(BOARD_SRC) $(CROSS_LDFLAGS) -o $@

$(BOARD_BIN): $(BOARD_ELF)
ifeq ($(CONFIG),esp32)
	$(CROSS_OBJCOPY) -O binary $< $@
else
	cp $< $@
endif

libttak: $(LIBTTAK_BOARD)

clean:
	rm -rf $(BUILD_DIR)
	# Do not remove libttak unless requested
