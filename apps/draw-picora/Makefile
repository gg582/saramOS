# draw-picora example app for saramOS.
# Ported from ~/zephyros-stm32/zephyr/draw_picora (a Zephyr/LVGL demo on
# the same STM32F769I-DISCO board): draws a 230x230 RGB565 sprite
# centered on the panel, and cycles through two alternate sprites while
# nudging its position each time the user (blue) button is pressed.
#
# Ported as a direct framebuffer blit rather than pulling in LVGL's
# image widget/decoder (which os/default's LVGL_SRC does not currently
# build) -- the three embedded sprites are plain RGB565 pixel arrays, so
# there's nothing an image widget would add here.
#
# Usage on the board: draw-picora; then press the blue user button (PA0)
# to cycle through the state machine.

OS_DIR := $(realpath ../../os/default)
BUILD_DIR := $(CURDIR)/build
APP_SRCS := $(realpath app_draw_picora.c) $(realpath img_src.c) \
            $(realpath img_parent.c) $(realpath img_brown.c)

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
