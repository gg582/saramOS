# Root Makefile for saramOS (STM32F769I-DISC1 only)
#
# Usage:
#   make                         # build the default example app (sudoku)
#   make APP_DIR=os/default      # build the base OS image only
#   make APP_DIR=<app-dir>       # build any app directory that has a Makefile
#   make flash                   # flash the current APP_DIR image
#   make clean                   # clean the current APP_DIR build

CONFIG ?= stm32f769i-disc1
BOARDS_CONFIG := configs/$(CONFIG)

ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error Configuration file $(BOARDS_CONFIG) not found)
endif

# Default representative example application.
APP_DIR ?= apps/example/game/sudoku

.PHONY: all app clean flash size

all: app

app:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) board

flash:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) flash

size:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) size

clean:
	-$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) clean
