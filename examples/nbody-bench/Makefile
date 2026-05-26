ROOT_DIR := $(realpath ../..)
BOARDS_CONFIG ?= $(ROOT_DIR)/boards.config
ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error boards.config not found at $(BOARDS_CONFIG))
endif
include $(BOARDS_CONFIG)

BUILD_DIR ?= build
BOARD ?= esp32-wroom32
SRCS := main.c nbody_bench.c
HAL_SRC := $(ROOT_DIR)/src/hal/esp32-wroom32/esp32_hal_stub.c
COMMON_SRCS := $(SRCS) $(HAL_SRC)
INCLUDES := -I. -I$(ROOT_DIR)/include

BOARD_BUILD_DIR := $(BUILD_DIR)/$(BOARD)
BOARD_ELF := $(BOARD_BUILD_DIR)/nbody_bench.elf
BOARD_BIN := $(BOARD_BUILD_DIR)/nbody_bench.bin

HOST_BUILD_DIR := $(BUILD_DIR)/native
HOST_APP := $(HOST_BUILD_DIR)/nbody_bench_host

CROSS_PREFIX ?= $(BOARD_TOOLCHAIN_PREFIX)
CROSS_CC ?= $(CROSS_PREFIX)gcc
CROSS_OBJCOPY ?= $(CROSS_PREFIX)objcopy
HOST_CC ?= gcc

ifneq ($(shell command -v $(CROSS_CC) 2>/dev/null),)
HAVE_CROSS_CC := 1
else
$(warning [nbody-bench] $(CROSS_CC) not found; falling back to host compiler $(HOST_CC))
CROSS_CC := $(HOST_CC)
HAVE_CROSS_CC := 0
endif

ifneq ($(shell command -v $(CROSS_OBJCOPY) 2>/dev/null),)
HAVE_CROSS_OBJCOPY := 1
else
$(warning [nbody-bench] $(CROSS_OBJCOPY) not found; falling back to objcopy)
CROSS_OBJCOPY := objcopy
HAVE_CROSS_OBJCOPY := 0
endif

CROSS_CFLAGS := -Os -ffunction-sections -fdata-sections -std=c17 -Wall -Wextra $(INCLUDES) \
                -DBOARD_TARGET=\"$(BOARD)\"
CROSS_CFLAGS += -DNBODY_DEPLOY_RUN
# Keep the xtensa sysroot/libraries ahead of host paths so ld picks the little-endian runtime.
CROSS_SYSROOT := $(strip $(shell $(CROSS_CC) --print-sysroot 2>/dev/null))
CROSS_LIBGCC := $(strip $(shell $(CROSS_CC) -print-libgcc-file-name 2>/dev/null))
CROSS_TOOLCHAIN_LIBS :=
ifneq ($(CROSS_SYSROOT),)
CROSS_TOOLCHAIN_LIBS += -L$(CROSS_SYSROOT)/lib -L$(CROSS_SYSROOT)/usr/lib
endif
ifneq ($(CROSS_LIBGCC),)
CROSS_TOOLCHAIN_LIBS += -L$(dir $(CROSS_LIBGCC))
endif
CROSS_LDFLAGS := -Wl,--gc-sections \
                 -Wl,--accept-unknown-input-arch \
                 -Wl,--no-warn-mismatch \
                 -Wl,--no-warn-search-mismatch \
                 -Wl,-m,elf32xtensa \
                 $(CROSS_TOOLCHAIN_LIBS)

HOST_CFLAGS := -Wall -Wextra -std=c17 $(INCLUDES) -pthread -DNATIVE_RUN
HOST_LDFLAGS := -lm -pthread

.PHONY: all board native-run clean

all: board

board: $(BOARD_BIN)

native-run: $(HOST_APP)
	./$(HOST_APP)

$(BOARD_BUILD_DIR):
	mkdir -p $(BOARD_BUILD_DIR)

$(HOST_BUILD_DIR):
	mkdir -p $(HOST_BUILD_DIR)

$(BOARD_ELF): $(BOARD_BUILD_DIR) $(COMMON_SRCS)
	$(CROSS_CC) $(CROSS_CFLAGS) $(COMMON_SRCS) -o $@ $(CROSS_LDFLAGS)

$(BOARD_BIN): $(BOARD_ELF)
ifeq ($(HAVE_CROSS_OBJCOPY),1)
	$(CROSS_OBJCOPY) -O binary $< $@
else
	cp $< $@
endif

$(HOST_APP): $(HOST_BUILD_DIR) $(COMMON_SRCS)
	$(HOST_CC) $(HOST_CFLAGS) $(COMMON_SRCS) -o $@ $(HOST_LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
