# mk/test-integration.mk -- end-to-end scenarios against the real product.
#
# Unlike the unit suite, these boot the exact build/me.xex a user runs, as
# AUTORUN.SYS on a DOS 2.5 disk, drive it with scripted keystrokes under
# libatari800, then read the files it saved back out of the disk image and
# compare them byte for byte.

INT_DIR := $(BUILD_DIR)/integration
INT_ATR := $(INT_DIR)/product.atr

SCENARIOS := content_editing file_operations

INT_DRIVER_SRCS := tests/integration/driver/atr_dos25.c tests/integration/driver/int_harness.c
INT_DRIVER_HDRS := tests/integration/driver/atr_dos25.h tests/integration/driver/int_harness.h
INT_HOST_CFLAGS := $(HOST_CFLAGS) -I tests/integration/driver $(LIBATARI800_CFLAGS) \
	-DHELP_SOURCE_PATH='"$(abspath data/emacs.hlp)"'

SCENARIO_BINS := $(addprefix $(INT_DIR)/scenario_,$(SCENARIOS))

$(INT_ATR): $(ME_XEX) data/emacs.hlp $(DISK_PREREQS)
	$(call msg,ATR,$@)
	$(Q)$(MKATR) 1040 $@ AUTORUN.SYS=$(ME_XEX) EMACS.HLP~data/emacs.hlp

$(INT_DIR)/scenario_%: tests/integration/scenarios/scenario_%.c \
		$(INT_DRIVER_SRCS) $(INT_DRIVER_HDRS) $(LIBATARI800)
	$(call msg,HOSTCC,$@)
	@mkdir -p $(@D)
	$(Q)$(HOST_CC) $(INT_HOST_CFLAGS) -o $@ $< $(INT_DRIVER_SRCS) \
		$(LIBATARI800) $(LIBATARI800_LIBS) -lm

# --- soak test (manual; not part of `make test`) ---------------------------

STRESS_BIN := $(INT_DIR)/stress_test
STRESS_SRCS := \
	tests/integration/stress/stress_test.c \
	tests/integration/driver/atr_dos25.c \
	tests/integration/driver/stress_harness.c \
	tests/driver/heapcheck.c
STRESS_HDRS := \
	tests/integration/driver/atr_dos25.h \
	tests/integration/driver/int_harness.h \
	tests/integration/driver/stress_harness.h \
	tests/driver/heapcheck.h

$(STRESS_BIN): $(STRESS_SRCS) $(STRESS_HDRS) $(LIBATARI800)
	$(call msg,HOSTCC,$@)
	@mkdir -p $(@D)
	$(Q)$(HOST_CC) $(INT_HOST_CFLAGS) -I tests/driver \
		-DSTRESS_MAP_PATH='"$(abspath $(ME_MAP))"' \
		-DSTRESS_OUT_DIR='"$(abspath $(INT_DIR))"' \
		-o $@ $(STRESS_SRCS) $(LIBATARI800) $(LIBATARI800_LIBS) -lm

# --- targets ----------------------------------------------------------------

.PHONY: build-integration test-integration stress

build-integration: $(SCENARIO_BINS) $(INT_ATR) ## Build the integration scenarios

# Each scenario writes files to its disk, so each gets a fresh copy.
test-integration: build-integration ## Run the integration scenarios
	$(Q)set -e; for s in $(SCENARIOS); do \
		echo "=== integration scenario: $$s ==="; \
		cp $(INT_ATR) $(INT_DIR)/scenario_$$s.atr; \
		$(INT_DIR)/scenario_$$s $(INT_DIR)/scenario_$$s.atr; \
	done

# Runs until stopped (Q+RETURN, Ctrl-C, or `touch build/integration/STOP_STRESS`).
# Optional: STRESS_CYCLES=<n> to stop after n cycles, STRESS_SEED=<n>.
stress: $(STRESS_BIN) $(INT_ATR) ## Run the heap/disk soak test (until stopped)
	$(Q)cp $(INT_ATR) $(INT_DIR)/stress.atr
	$(Q)$(STRESS_BIN) $(INT_DIR)/stress.atr $(or $(STRESS_CYCLES),0) $(STRESS_SEED)
