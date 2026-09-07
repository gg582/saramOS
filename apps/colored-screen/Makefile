# Colored-screen diagnostic app for saramOS.
# Builds the default OS image with a "colortest" command linked in that
# cycles the whole panel through solid full-screen colors, to make any
# DSI/LTDC positional or color-channel corruption trivial to point at
# (which color, which region) instead of describing a complex picture.

OS_DIR := $(realpath ../../os/default)
BUILD_DIR := $(CURDIR)/build
APP_SRCS := $(realpath app_colored_screen.c)

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
