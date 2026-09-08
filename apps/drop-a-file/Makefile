# drop-a-file example app for saramOS.
# Serves a "Drop files to draw" web page over the existing DHCP/lwIP-httpd
# stack, saves the dropped BMP to the SD card via FatFS, and draws it
# (scaled to fit, letterboxed over a user-picked background color) on the
# 800x480 panel via the existing hal_display_* API.
#
# Usage on the board: net init; sd init; http start; dropafile
# then browse to http://<board-ip>/ from a machine on the same network.

OS_DIR := $(realpath ../../os/default)
BUILD_DIR := $(CURDIR)/build
APP_SRCS := $(realpath app_drop_a_file.c)

.PHONY: all board clean flash size

all: board

board:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)" TOOLS="$(TOOLS)"

flash:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)" TOOLS="$(TOOLS)" flash

size:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)" TOOLS="$(TOOLS)" size

clean:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)" TOOLS="$(TOOLS)" clean
