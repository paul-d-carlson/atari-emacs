# atari-emacs -- MicroEMACS 3.6 for the Atari 8-bit
#
# Run `make help` for the list of targets. All output goes to $(BUILD_DIR).

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SUFFIXES:

VERSION := 0.1.0

include mk/config.mk
include mk/deps.mk

# ---------------------------------------------------------------------------
# Editor sources
# ---------------------------------------------------------------------------
# Link order is significant: it fixes the memory layout, and the unit-test
# image (mk/test-unit.mk) relies on the same objects.
APP_SRCS := \
	src/atari.c \
	src/basic.c \
	src/bind.c \
	src/buffer.c \
	src/display.c \
	src/file.c \
	src/fileio.c \
	src/line.c \
	src/main.c \
	src/random.c \
	src/search.c \
	src/termio.c \
	src/window.c

APP_CFG := src/atari.cfg
OBJ_DIR := $(BUILD_DIR)/obj

app_obj = $(patsubst %.c,$(OBJ_DIR)/%.o,$(1))

ME_XEX := $(BUILD_DIR)/me.xex
ME_MAP := $(BUILD_DIR)/me.map
ME_DBG := $(BUILD_DIR)/me.dbg
ME_ATR := $(BUILD_DIR)/me.atr

$(OBJ_DIR)/src/%.o: src/%.c
	$(call msg,CC65,$<)
	@mkdir -p $(@D)
	$(Q)$(CL65) $(APP_CFLAGS) --create-dep $(@:.o=.d) -c -o $@ $<

$(ME_XEX): $(call app_obj,$(APP_SRCS)) $(APP_CFG)
	$(call msg,LD65,$@)
	$(Q)$(CL65) -t $(TARGET) -C $(APP_CFG) -m $(ME_MAP) -Wl --dbgfile,$(ME_DBG) \
		-o $@ $(call app_obj,$(APP_SRCS))
	@printf '  %-8s %s: %s bytes\n' SIZE $@ "$$(wc -c <$@ | tr -d ' ')"

# ---------------------------------------------------------------------------
# Boot disk
# ---------------------------------------------------------------------------
MKATR := sh tools/mkatr.sh $(DIR2ATR) $(DOS25_DIR)

# Every disk image depends on these: a verified DOS 2.5 and a linker
# config that loads the editor above DOS's MEMLO.
DISK_PREREQS := $(BUILD_DIR)/.dos25-verified $(BUILD_DIR)/.memlo-checked $(DIR2ATR_DEP)

DOS25_VERIFY ?= 1
$(BUILD_DIR)/.dos25-verified: $(DOS25_DIR)/DOS.SYS $(DOS25_DIR)/DUP.SYS third_party/atari-dos25/SHA256SUMS
	@mkdir -p $(@D)
ifeq ($(DOS25_VERIFY),1)
	$(Q)cd $(DOS25_DIR) && $(SHA256SUM) -c $(abspath third_party/atari-dos25/SHA256SUMS) >/dev/null \
		|| { echo "error: DOS.SYS/DUP.SYS in $(DOS25_DIR) are not the expected Atari DOS 2.5." >&2; \
		     echo "       A different DOS build may use more low memory than the editor" >&2; \
		     echo "       allows for. Override with DOS25_VERIFY=0 at your own risk." >&2; exit 1; }
endif
	$(Q)touch $@

# The editor is linked at STARTADDRESS; DOS 2.5 owns everything below MEMLO.
$(BUILD_DIR)/.memlo-checked: $(APP_CFG)
	@mkdir -p $(@D)
	$(Q)start=$$(sed -n 's/.*STARTADDRESS: *default *= *\$$\([0-9A-Fa-f]*\).*/\1/p' $<); \
	if [ -z "$$start" ]; then echo "error: no STARTADDRESS in $<" >&2; exit 1; fi; \
	if [ $$((0x$$start)) -le $$((0x$(DOS25_MEMLO))) ]; then \
		echo "error: $< STARTADDRESS \$$$$start is not above DOS 2.5 MEMLO \$$$(DOS25_MEMLO)" >&2; exit 1; \
	fi
	$(Q)touch $@

$(ME_ATR): $(ME_XEX) data/emacs.hlp data/welcome.txt $(DISK_PREREQS)
	$(call msg,ATR,$@)
	$(Q)$(MKATR) 720 $@ AUTORUN.SYS=$(ME_XEX) EMACS.HLP~data/emacs.hlp WELCOME.TXT~data/welcome.txt

# ---------------------------------------------------------------------------
# Release archive
# ---------------------------------------------------------------------------
DIST_NAME := atari-emacs-$(VERSION)
DIST_ZIP  := $(BUILD_DIR)/$(DIST_NAME).zip

$(DIST_ZIP): $(ME_XEX) $(ME_ATR) README.md LICENSE NOTICE
	$(call msg,ZIP,$@)
	$(Q)rm -rf $(BUILD_DIR)/$(DIST_NAME) $@
	$(Q)mkdir -p $(BUILD_DIR)/$(DIST_NAME)
	$(Q)cp $^ $(BUILD_DIR)/$(DIST_NAME)/
	$(Q)cd $(BUILD_DIR) && zip -qr $(DIST_NAME).zip $(DIST_NAME)
	$(Q)rm -rf $(BUILD_DIR)/$(DIST_NAME)

# ---------------------------------------------------------------------------
# User-facing targets
# ---------------------------------------------------------------------------
.PHONY: all atr run dist clean distclean help

all: $(ME_XEX) ## Build build/me.xex (default)

atr: $(ME_ATR) ## Build bootable DOS 2.5 disk build/me.atr

run: $(ME_ATR) ## Boot build/me.atr in the atari800 emulator
	$(ATARI800) -xl -nobasic $(ME_ATR)

dist: $(DIST_ZIP) ## Package me.xex and me.atr for a release

clean: ## Remove build output (keeps downloaded dependencies)
	$(Q)if [ -d $(BUILD_DIR) ]; then \
		find $(BUILD_DIR) -mindepth 1 -maxdepth 1 ! -name deps -exec rm -rf {} +; \
	fi

distclean: ## Remove all build output, including dependencies
	$(Q)rm -rf $(BUILD_DIR)

help: ## Show this help
	@echo "Usage: make [target] [VARIABLE=value ...]"
	@echo
	@echo "Targets:"
	@grep -hE '^[a-z-]+:.*## ' $(MAKEFILE_LIST) | \
		sed 's/^\([a-z-]*\):.*## /\1|/' | awk -F'|' '{ printf "  %-18s %s\n", $$1, $$2 }'
	@echo
	@echo "Common variables:"
	@echo "  CL65=path          cc65 compiler driver      (default: cl65)"
	@echo "  HOST_CC=compiler   compiler for test drivers (default: cc)"
	@echo "  DOS25_DIR=dir      DOS.SYS/DUP.SYS location  (default: third_party/atari-dos25)"
	@echo "  DIR2ATR=path       use an installed dir2atr instead of building one"
	@echo "  V=1                show full commands"

-include $(shell find $(OBJ_DIR) -name '*.d' 2>/dev/null)
