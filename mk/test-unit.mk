# mk/test-unit.mk -- the resident/overlay unit-test suite.
#
# One resident 6502 image links the real editor (minus main.c), the test
# harness, an ABI export table and an overlay loader. Each test suite is a
# small overlay loaded from disk into a fixed window at run time, calling
# into the resident image only through the ABI table. A native driver
# boots the disk under libatari800, scripts the keyboard, reads results
# out of emulated RAM and reports pass/fail, heap use and line coverage.
# See tests/README.md for the full design.

UNIT_DIR    := $(BUILD_DIR)/unit
UNIT_OBJ    := $(UNIT_DIR)/obj
UNIT_CFLAGS := -I tests/target -I src

# The resident image's link order fixes the addresses that the overlays
# and the ABI table assume. Do not reorder.
RES_HARNESS_SRCS := \
	tests/target/test_main_resident.c \
	tests/target/harness.c \
	tests/target/harness_common.c \
	tests/target/abi_export.c \
	tests/target/abi_hdr.s \
	tests/target/overlay_loader.c
RES_APP_SRCS := \
	src/buffer.c src/display.c src/file.c src/fileio.c src/line.c \
	src/window.c src/atari.c src/termio.c src/basic.c src/random.c \
	src/bind.c src/search.c

RES_CFG := tests/target/test-resident.cfg
OVL_CFG := tests/target/test-overlay.cfg
RES_XEX := $(UNIT_DIR)/test_resident.xex
RES_MAP := $(UNIT_DIR)/test_resident.map
RES_DBG := $(UNIT_DIR)/test_resident.dbg
RES_ATR := $(UNIT_DIR)/test_resident.atr
RES_RUN_ATR := $(UNIT_DIR)/run/test_resident.atr
RES_OBJS := \
	$(patsubst tests/target/%,$(UNIT_OBJ)/%.o,$(RES_HARNESS_SRCS)) \
	$(call app_obj,$(RES_APP_SRCS))

# suite:DOS-filename. The 8.3 names are what test_main_resident.c loads.
OVERLAYS := \
	buffer:OVBUF    atari:OVATA     basic:OVBAS     bind:OVBND \
	display:OVDSP   display2:OVDS2  file1:OVFIL1    file2:OVFIL2 \
	file3:OVFIL3    fileio:OVFIO    line1:OVLIN1    line2:OVLIN2 \
	random1:OVRAN1  random2:OVRAN2  random3:OVRAN3  search:OVSCH \
	window:OVWIN    termio:OVTIO
ovl_suite = $(word 1,$(subst :, ,$(1)))
ovl_dos   = $(word 2,$(subst :, ,$(1)))
OVL_BINS  := $(foreach o,$(OVERLAYS),$(UNIT_DIR)/ov_$(call ovl_suite,$(o)).bin)
OVL_OBJS  := $(patsubst $(UNIT_DIR)/%.bin,$(UNIT_OBJ)/%.o,$(OVL_BINS))
.SECONDARY: $(OVL_OBJS)

# Headers every overlay depends on (the ABI wiring and harness contract).
OVL_HDRS := tests/target/resident_api.h tests/target/abi_table.h tests/target/harness.h

UNIT_RUNNER := $(UNIT_DIR)/test_runner
UNIT_RUNNER_SRCS := \
	tests/driver/test_runner_resident.c \
	tests/driver/harness_runner.c \
	tests/driver/dbgcov.c \
	tests/driver/heapcheck.c
UNIT_RUNNER_HDRS := \
	tests/driver/harness_runner.h tests/driver/dbgcov.h \
	tests/driver/heapcheck.h tests/common/test_protocol.h

# --- 6502 side --------------------------------------------------------------

$(UNIT_OBJ)/%.c.o: tests/target/%.c
	$(call msg,CC65,$<)
	@mkdir -p $(@D)
	$(Q)$(CL65) $(APP_CFLAGS) $(UNIT_CFLAGS) --create-dep $(@:.o=.d) -c -o $@ $<

$(UNIT_OBJ)/%.s.o: tests/target/%.s
	$(call msg,CA65,$<)
	@mkdir -p $(@D)
	$(Q)$(CL65) -t $(TARGET) -g -c -o $@ $<

$(RES_XEX): $(RES_OBJS) $(RES_CFG)
	$(call msg,LD65,$@)
	$(Q)$(CL65) -t $(TARGET) -C $(RES_CFG) -m $(RES_MAP) -Wl --dbgfile,$(RES_DBG) \
		-o $@ $(RES_OBJS)

# Overlays are built without -Or/--register-space: they hold only test code.
$(UNIT_OBJ)/ov_%.o: tests/target/test_%.c $(OVL_HDRS)
	$(call msg,CC65,$<)
	@mkdir -p $(@D)
	$(Q)$(CL65) -t $(TARGET) -Os $(UNIT_CFLAGS) --create-dep $(@:.o=.d) -c -o $@ $<

$(UNIT_DIR)/ov_%.bin: $(UNIT_OBJ)/ov_%.o $(OVL_CFG)
	$(call msg,LD65,$@)
	$(Q)$(CL65) -t $(TARGET) -C $(OVL_CFG) -m $(@:.bin=.map) -o $@ $<

$(RES_ATR): $(RES_XEX) $(OVL_BINS) $(DISK_PREREQS)
	$(call msg,ATR,$@)
	$(Q)$(MKATR) 1040 $@ AUTORUN.SYS=$(RES_XEX) \
		$(foreach o,$(OVERLAYS),$(call ovl_dos,$(o)).BIN=$(UNIT_DIR)/ov_$(call ovl_suite,$(o)).bin)

# --- host side --------------------------------------------------------------

$(UNIT_RUNNER): $(UNIT_RUNNER_SRCS) $(UNIT_RUNNER_HDRS) $(LIBATARI800)
	$(call msg,HOSTCC,$@)
	@mkdir -p $(@D)
	$(Q)$(HOST_CC) $(HOST_CFLAGS) -I tests/driver $(LIBATARI800_CFLAGS) \
		-o $@ $(UNIT_RUNNER_SRCS) $(LIBATARI800) $(LIBATARI800_LIBS) -lm

# --- targets ----------------------------------------------------------------

.PHONY: build-unit test-unit

build-unit: $(UNIT_RUNNER) $(RES_ATR) ## Build the unit-test image and driver

# Tests write to the disk, so each run gets a fresh copy of the image.
test-unit: build-unit ## Run the unit tests
	@mkdir -p $(dir $(RES_RUN_ATR))
	$(Q)cp $(RES_ATR) $(RES_RUN_ATR)
	$(Q)$(UNIT_RUNNER) $(RES_RUN_ATR) $(RES_DBG)

-include $(shell find $(UNIT_OBJ) -name '*.d' 2>/dev/null)
