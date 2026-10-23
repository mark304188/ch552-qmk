# Makefile - CH552 QMK/VIA keyboard firmware
#
# Targets: all (default), flash, size, clean
# Board:   BOARD=default (3x4 matrix) or BOARD=4key (direct 4-key board)
#
# Requires SDCC 4.x.  The CH552 has 16 KB flash; the upper 2 KB
# (0x3800-0x3FFF) is the WCH ISP bootloader, so application code is
# limited to 0x3800 bytes.

BOARD ?= default

ifeq ($(BOARD),4key)
PROJECT := ch552_qmk_4key
BOARD_CFLAGS := -DBOARD_4KEY
else
PROJECT := ch552_qmk
BOARD_CFLAGS :=
endif

.DEFAULT_GOAL := all


VCC ?= 5V
ifeq ($(VCC),3V3)
FREQ_SYS ?= 12000000
else
FREQ_SYS ?= 16000000
endif

# 14 KB application area.
CODE_SIZE ?= 0x3800

# 1 KB on-chip xRAM.  The USB endpoint buffers occupy 0x0000-0x00FF,
# so the compiler gets 0x0100-0x03FF (768 bytes).
XRAM_SIZE ?= 0x0300
XRAM_LOC  ?= 0x0100

SDCC ?= sdcc
PACKIHX ?= packihx
OBJCOPY ?= objcopy
FLASHER ?= ch55xtool

SRC_DIR := firmware/src
INC_DIR := firmware/include
BUILD_DIR := build

C_FILES := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/usb.c \
	$(SRC_DIR)/keyboard.c \
	$(SRC_DIR)/matrix.c \
	$(SRC_DIR)/via.c \
	$(SRC_DIR)/rgb.c \
	$(SRC_DIR)/ws2812.c \
	$(SRC_DIR)/eeprom.c \
	$(SRC_DIR)/debug.c

INCLUDES := -I$(INC_DIR)

CFLAGS := \
	-mmcs51 --model-small \
	--xram-size $(XRAM_SIZE) --xram-loc $(XRAM_LOC) \
	--code-size $(CODE_SIZE) \
	-DFREQ_SYS=$(FREQ_SYS) \
	$(BOARD_CFLAGS) \
	$(INCLUDES) \
	--std-sdcc11 --opt-code-speed

CFLAGS += $(EXTRA_FLAGS)

RELS := $(C_FILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.rel)
HDRS := $(wildcard $(INC_DIR)/*.h)

CFG_STAMP := $(BUILD_DIR)/.config

.PHONY: all flash size clean FORCE

FORCE:

$(CFG_STAMP): FORCE
	@mkdir -p $(BUILD_DIR)
	@echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@

all: $(BUILD_DIR)/$(PROJECT).bin $(BUILD_DIR)/$(PROJECT).hex

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.rel: $(SRC_DIR)/%.c $(HDRS) $(CFG_STAMP) | $(BUILD_DIR)
	$(SDCC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/$(PROJECT).ihx: $(RELS)
	cd $(BUILD_DIR) && $(SDCC) $(notdir $(RELS)) $(CFLAGS) -o $(PROJECT).ihx

$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).ihx
	$(PACKIHX) $< > $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).ihx
	$(OBJCOPY) -I ihex -O binary $< $@

size: $(BUILD_DIR)/$(PROJECT).ihx
	@size $(BUILD_DIR)/$(PROJECT).ihx 2>/dev/null || true
	@awk '/ROM\/EPROM|CODE/ {print}' $(BUILD_DIR)/$(PROJECT).map 2>/dev/null || true

flash: $(BUILD_DIR)/$(PROJECT).bin
	$(FLASHER) -f $< -r

clean:
	rm -rf $(BUILD_DIR)
