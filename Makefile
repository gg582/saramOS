# Root Makefile for saramOS
#
# Usage:
#   make                         # build the default example app (sudoku)
#   make BOARD=stm32f769i-disc1  # build for the DISC1 variant
#   make APP_DIR=os/default      # build the base OS image only
#   make APP_DIR=<app-dir>       # build any app directory that has a Makefile
#   make flash                   # flash the current APP_DIR image
#   make clean                   # clean the current APP_DIR build

BOARD ?= stm32f769i-disco
BOARDS_CONFIG := configs/$(BOARD)

ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error Configuration file $(BOARDS_CONFIG) not found)
endif

# Default representative example application.
APP_DIR ?= apps/example/game/sudoku

.PHONY: all app clean flash size

all: app

app:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) BOARD=$(BOARD) TOOLS=$(TOOLS) board

flash:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) BOARD=$(BOARD) TOOLS=$(TOOLS) flash

size:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) BOARD=$(BOARD) TOOLS=$(TOOLS) size

clean:
	-$(MAKE) -C $(APP_DIR) BOARD=$(BOARD) TOOLS=$(TOOLS) clean
