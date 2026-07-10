# Graphical shell app for saramOS
# Builds the default OS image with LVGL and the graphical shell linked in.

OS_DIR := $(realpath ../../os/default)
BUILD_DIR := $(CURDIR)/build
APP_SRCS := $(realpath app_graphical_shell.c)

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
