ROOT_DIR := $(realpath ../..)
BOARD ?= stm32f769i-disco
BOARDS_CONFIG := $(ROOT_DIR)/configs/$(BOARD)

ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error Configuration file $(BOARDS_CONFIG) not found)
endif

include $(BOARDS_CONFIG)

BUILD_DIR ?= build
BOARD_BUILD_DIR := $(BUILD_DIR)/$(BOARD)

HAL_DIR := $(ROOT_DIR)/src/hal/$(BOARD)
HAL_SRC := $(HAL_DIR)/hal_uart.c $(HAL_DIR)/hal_sys.c \
           $(HAL_DIR)/hal_gpio.c $(HAL_DIR)/hal_eth.c $(HAL_DIR)/hal_sdmmc.c

# Extra display/SDRAM/LVGL port sources for the graphical-shell app.
GFX_HAL_SRC := $(HAL_DIR)/hal_sdram.c $(HAL_DIR)/hal_display.c $(HAL_DIR)/lvgl_port.c

LVGL_DIR := $(ROOT_DIR)/third_party/lvgl
LVGL_SRC := $(LVGL_DIR)/src/lvgl.c \
            $(LVGL_DIR)/src/misc/lv_style.c \
            $(LVGL_DIR)/src/font/lv_font.c \
            $(LVGL_DIR)/src/font/lv_font_unscii_8.c \
            $(LVGL_DIR)/src/core/lv_display.c \
            $(LVGL_DIR)/src/core/lv_obj.c \
            $(LVGL_DIR)/src/widgets/lv_textarea.c \
            $(LVGL_DIR)/src/widgets/lv_label.c \
            $(LVGL_DIR)/src/draw/lv_draw.c

STARTUP := $(HAL_DIR)/startup.S
LD_SCRIPT := $(HAL_DIR)/linker.ld

OS_DIR := $(ROOT_DIR)/src/os
OS_SRC := $(OS_DIR)/saramos_arena.c $(OS_DIR)/saramos_owner.c \
          $(OS_DIR)/saramos_kernel.c $(OS_DIR)/saramos_context.S \
          $(OS_DIR)/saramos_process.c $(OS_DIR)/saramos_scheduler.c \
          $(OS_DIR)/saramos_pipe.c

LWIP_DIR := $(ROOT_DIR)/third_party/lwip
LWIP_SRC := $(LWIP_DIR)/src/core/init.c \
            $(LWIP_DIR)/src/core/netif.c \
            $(LWIP_DIR)/src/core/inet_chksum.c \
            $(LWIP_DIR)/src/core/ip.c \
            $(LWIP_DIR)/src/core/mem.c \
            $(LWIP_DIR)/src/core/memp.c \
            $(LWIP_DIR)/src/core/pbuf.c \
            $(LWIP_DIR)/src/core/stats.c \
            $(LWIP_DIR)/src/core/sys.c \
            $(LWIP_DIR)/src/core/tcp.c \
            $(LWIP_DIR)/src/core/tcp_in.c \
            $(LWIP_DIR)/src/core/tcp_out.c \
            $(LWIP_DIR)/src/core/timeouts.c \
            $(LWIP_DIR)/src/core/udp.c \
            $(LWIP_DIR)/src/core/def.c \
            $(LWIP_DIR)/src/core/ipv4/acd.c \
            $(LWIP_DIR)/src/core/ipv4/dhcp.c \
            $(LWIP_DIR)/src/core/ipv4/etharp.c \
            $(LWIP_DIR)/src/core/ipv4/icmp.c \
            $(LWIP_DIR)/src/core/ipv4/ip4.c \
            $(LWIP_DIR)/src/core/ipv4/ip4_addr.c \
            $(LWIP_DIR)/src/core/ipv4/ip4_frag.c \
            $(LWIP_DIR)/src/netif/ethernet.c \
            $(LWIP_DIR)/src/apps/http/httpd.c \
            $(LWIP_DIR)/src/apps/http/fs.c

FATFS_DIR := $(ROOT_DIR)/third_party/fatfs/source
FATFS_SRC := $(FATFS_DIR)/ff.c \
             $(FATFS_DIR)/ffsystem.c \
             $(FATFS_DIR)/ffunicode.c

LIBTTAK_DIR := $(ROOT_DIR)/engine/libttak
LIBTTAK_A := $(LIBTTAK_DIR)/lib/libttak.a

ENABLE_BUILTIN_EXAMPLES ?= 1
TOOLS ?=
TOOLS_LIST := $(shell echo '$(TOOLS)' | tr ',' ' ')

# Metapackage: shell => full userland environment
ifneq ($(filter shell,$(TOOLS_LIST)),)
EFFECTIVE_TOOLS := fsutils vi coreutils extrashell
else
EFFECTIVE_TOOLS := $(TOOLS_LIST)
endif

APP_SRC := main.c syscalls.c \
           ethernetif.c http_server.c sd_diskio.c \
           $(OS_SRC) $(LWIP_SRC) $(FATFS_SRC)

ifeq ($(ENABLE_BUILTIN_EXAMPLES),1)
APP_SRC += builtin_programs.c
CFLAGS_EXTRA += -DENABLE_BUILTIN_EXAMPLES=1
endif

ifneq ($(filter fsutils,$(EFFECTIVE_TOOLS)),)
APP_SRC += $(wildcard $(ROOT_DIR)/tools/fsutils/*.c)
CFLAGS_EXTRA += -DENABLE_TOOL_FSUTILS=1
endif

ifneq ($(filter vi,$(EFFECTIVE_TOOLS)),)
APP_SRC += $(wildcard $(ROOT_DIR)/tools/saramos_vi/*.c)
CFLAGS_EXTRA += -DENABLE_TOOL_VI=1
endif

ifneq ($(filter coreutils,$(EFFECTIVE_TOOLS)),)
APP_SRC += $(wildcard $(ROOT_DIR)/tools/coreutils/*.c)
CFLAGS_EXTRA += -DENABLE_TOOL_COREUTILS=1
endif

ifneq ($(filter extrashell,$(EFFECTIVE_TOOLS)),)
APP_SRC += shell.c $(wildcard $(ROOT_DIR)/tools/shell/*.c)
CFLAGS_EXTRA += -DENABLE_TOOL_EXTRASHELL=1
else ifneq ($(filter fsutils,$(EFFECTIVE_TOOLS)),)
APP_SRC += shell.c
endif

APP_SRC += $(EXTRA_APP_SRCS)

# If the graphical-shell app is being built, include LVGL and display HAL.
ifeq ($(findstring app_graphical_shell.c,$(EXTRA_APP_SRCS)),app_graphical_shell.c)
HAL_SRC += $(GFX_HAL_SRC)
APP_SRC += $(LVGL_SRC)
CFLAGS_EXTRA += -DENABLE_GRAPHICAL_SHELL=1
endif

INCLUDES := -I$(ROOT_DIR)/include \
            -I$(HAL_DIR) \
            -I$(LIBTTAK_DIR)/include \
            -I$(LWIP_DIR)/src/include \
            -I$(FATFS_DIR) \
            -I$(ROOT_DIR)/tools/fsutils \
            -I$(ROOT_DIR)/tools/saramos_vi \
            -I$(ROOT_DIR)/tools/coreutils \
            -I$(ROOT_DIR)/tools/shell \
            -I$(LVGL_DIR) \
            -I.

CROSS_CC := $(BOARD_CC)
CROSS_OBJCOPY := $(BOARD_OBJCOPY)
CROSS_SIZE := $(BOARD_SIZE)

CFLAGS := $(BOARD_CFLAGS) $(INCLUDES) $(CFLAGS_EXTRA) -std=c17
ASFLAGS := $(BOARD_CFLAGS) $(INCLUDES)
LDFLAGS := $(BOARD_LDFLAGS) -T$(LD_SCRIPT) -L$(LIBTTAK_DIR)/lib -lttak \
           -Wl,-Map=$(BOARD_BUILD_DIR)/saramos.map

BOARD_ELF := $(BOARD_BUILD_DIR)/saramos.elf
BOARD_BIN := $(BOARD_BUILD_DIR)/saramos.bin
BOARD_HEX := $(BOARD_BUILD_DIR)/saramos.hex

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
