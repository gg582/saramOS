# Root Makefile for saramOS (STM32F769I-DISC1 only)

CONFIG ?= stm32f769i-disc1
BOARDS_CONFIG := configs/$(CONFIG)

ifeq ($(wildcard $(BOARDS_CONFIG)),)
$(error Configuration file $(BOARDS_CONFIG) not found)
endif

include $(BOARDS_CONFIG)

APP_DIR ?= examples/helloworld

.PHONY: all app clean flash size

all: app

app:
	@if [ ! -d "$(APP_DIR)" ]; then echo "Error: APP_DIR=$(APP_DIR) not found"; exit 1; fi
	$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) board

flash:
	$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) flash

size:
	$(MAKE) -C $(APP_DIR) CONFIG=$(CONFIG) size

clean:
	-$(MAKE) -C $(APP_DIR) clean
