# Sudoku app for saramOS
# Builds the default OS image with this application's sources linked in.

OS_DIR := $(realpath ../../../..)/os/default
BUILD_DIR := $(CURDIR)/build
APP_SRCS := $(realpath sudoku.c app_sudoku.c)

.PHONY: all board clean flash size

all: board

board:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)"

flash:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)" flash

size:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)" size

clean:
	$(MAKE) -C $(OS_DIR) EXTRA_APP_SRCS="$(APP_SRCS)" BUILD_DIR="$(BUILD_DIR)" clean
