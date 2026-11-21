# mk/config.mk -- tools, paths and flags shared by every build fragment.
#
# Every variable here can be overridden on the command line, e.g.
#   make CL65=/opt/cc65/bin/cl65 HOST_CC=clang DOS25_DIR=~/atari/dos25

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
BUILD_DIR ?= build
DEPS_DIR  ?= $(BUILD_DIR)/deps
DOS25_DIR ?= third_party/atari-dos25

# ---------------------------------------------------------------------------
# Atari (6502) toolchain -- cc65 <https://cc65.github.io/>
# ---------------------------------------------------------------------------
CL65   ?= cl65
TARGET := atari

# Flags for the editor itself (and for the copy of it linked into the unit
# test image). These are tuned for size; changing them changes the memory
# map, so re-run `make test` and compare the heap report after any change.
#   -Os                  inline small runtime helpers
#   -Or                  honour `register` (6502 zero-page pseudo-registers)
#   --register-space 24  12 ZP register pairs; Atari OS ZP use ends at $81,
#                        so $82-$AD is free. Savings plateau at 24 bytes.
APP_CFLAGS := -t $(TARGET) -Os -Or --register-space 24 -g

# ---------------------------------------------------------------------------
# Host toolchain (test drivers, which embed the libatari800 emulator)
# ---------------------------------------------------------------------------
HOST_CC     ?= cc
HOST_CFLAGS ?= -O2 -Wall

# ---------------------------------------------------------------------------
# Miscellaneous host tools
# ---------------------------------------------------------------------------
CURL     ?= curl
ATARI800 ?= atari800

ifneq ($(shell command -v sha256sum 2>/dev/null),)
SHA256SUM ?= sha256sum
else
SHA256SUM ?= shasum -a 256
endif

# ---------------------------------------------------------------------------
# Memory-map guard
# ---------------------------------------------------------------------------
# MEMLO (first free byte) after Atari DOS 2.5 boots with its default
# configuration, measured under emulation. The editor must load above it.
DOS25_MEMLO := 1CFC

# Quiet output by default; `make V=1` shows full commands.
ifeq ($(V),1)
Q :=
msg :=
else
Q := @
msg = @printf '  %-8s %s\n' '$(1)' '$(2)'
endif
