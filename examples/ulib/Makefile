# SPDX-License-Identifier: GPL-2.0+
#
# Standalone Makefile for U-Boot library examples
#
# Copyright 2025 Canonical Ltd.
# Written by Simon Glass <simon.glass@canonical.com>

# This Makefile can be used to build the examples. See doc/develop/ulib.rst
#
# Usage: cd examples/ulib; make UBOOT_BUILD=/tmp/b/sandbox/ srctree=../..
#
# This Makefile is also called from the main U-Boot Makefile when CONFIG_ULIB
# and CONFIG_EXAMPLES are enabled. It receives these variables, many of which
# are needed to ensure that the output goes to the right place:
#
#   UBOOT_BUILD    - Build directory (e.g., /tmp/b/sandbox)
#   EXAMPLE_DIR    - Source tree path for these examples
#   OUTDIR         - Output directory for object files and executables
#   srctree        - U-Boot source tree
#
# Also these may be provided:
#   CC             - C compiler
#   CFLAGS         - C compiler flags
#   SDL_CONFIG     - Name of sdl2-config program
#   PLATFORM_LIBS  - Platform-specific libraries
#   LIB_STATIC_LDS - Linker script for static library

# For standalone builds, provide default values
EXAMPLE_DIR ?= .
OUTDIR ?= .
CC ?= gcc
SDL_CONFIG ?= sdl2-config
PLATFORM_LIBS ?= $(shell $(SDL_CONFIG) --libs)
LIB_STATIC_LDS ?= static.lds
# The main Makefile passes in Q=@ for quiet output
Q ?=

DEMO_SRC := $(EXAMPLE_DIR)/demo.c
DEMO_BINS := $(OUTDIR)/demo $(OUTDIR)/demo_static

# Default target builds both programs
all: $(DEMO_BINS)

# Create the output directory if it doesn't exist
$(OUTDIR):
	@mkdir -p $@

# The U-Boot library must be built before we can link against it. This is
# signalled by the presence of the $(UBOOT_BUILD)/examples/ulib directory.
# This is an order-only prerequisite, so it does not trigger a rebuild if the
# timestamp of the directory changes.
$(DEMO_BINS): | $(UBOOT_BUILD)/examples/ulib $(OUTDIR)

# Build demo (dynamically linked with libu-boot.so)
$(OUTDIR)/demo: $(DEMO_SRC)
	$(CC) $(CFLAGS) \
		-idirafter$(srctree)/include -o $@ $< \
		-L$(UBOOT_BUILD) -lu-boot \
		-Wl,-rpath,$(UBOOT_BUILD)

# Build demo_static (statically linked with libu-boot.a)
$(OUTDIR)/demo_static: $(DEMO_SRC)
	$(CC) $(CFLAGS) \
		-idirafter$(srctree)/include -o $@ $< \
		-Wl,-T,$(LIB_STATIC_LDS) \
		-Wl,--whole-archive $(UBOOT_BUILD)/libu-boot.a -Wl,--no-whole-archive \
		-lpthread -ldl $(PLATFORM_LIBS) -Wl,-z,noexecstack

clean:
	$(Q)rm -f $(DEMO_BINS)

.PHONY: all clean
