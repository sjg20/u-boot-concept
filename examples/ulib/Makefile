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

# Common compiler flags for programs using system headers first
SYSTEM_CFLAGS := -I$(UBOOT_BUILD)/include -idirafter$(srctree)/include \
		-include $(srctree)/include/linux/compiler_attributes.h

# Common compiler flags for programs using U-Boot headers first (like U-Boot
# internal build)
UBOOT_CFLAGS := -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
	-I$(UBOOT_BUILD)/include \
	-I$(srctree)/include \
	-I$(srctree)/arch/sandbox/include \
	-include $(UBOOT_BUILD)/include/config.h \
	-include $(srctree)/include/linux/kconfig.h \
	-I$(srctree)/dts/upstream/include \
	-D__KERNEL__ -DCONFIG_SYS_TEXT_BASE=0

SHARED_LDFLAGS := -L$(UBOOT_BUILD) -lu-boot -Wl,-rpath,$(UBOOT_BUILD)

STATIC_LDFLAGS := -Wl,-T,$(LIB_STATIC_LDS) \
	-Wl,--whole-archive $(UBOOT_BUILD)/libu-boot.a -Wl,--no-whole-archive \
	-lpthread -ldl $(PLATFORM_LIBS) -Wl,-z,noexecstack

# Program definitions - can be single file or multi-object
DEMO_SRC := $(EXAMPLE_DIR)/demo.c
demo-objs := demo.o demo_helper.o
DEMO_BINS := $(OUTDIR)/demo $(OUTDIR)/demo_static

ALL_BINS := $(DEMO_BINS)

# Default target builds both programs
all: $(ALL_BINS)

# Create the output directory if it doesn't exist
$(OUTDIR):
	@mkdir -p $@

# Pattern rule for building object files with system headers first (default)
$(OUTDIR)/%.o: $(EXAMPLE_DIR)/%.c | $(OUTDIR)
	$(CC) $(CFLAGS) $(SYSTEM_CFLAGS) -c -o $@ $<

# Pattern rule for building object files with U-Boot headers first
$(OUTDIR)/%_uboot.o: $(EXAMPLE_DIR)/%.c | $(OUTDIR)
	$(CC) $(CFLAGS) $(UBOOT_CFLAGS) -c -o $@ $<

# The U-Boot library must be built before we can link against it
# Order-only prerequisites ensure libraries exist before linking
$(ALL_BINS): | $(UBOOT_BUILD)/libu-boot.a $(UBOOT_BUILD)/libu-boot.so $(OUTDIR)

# Build demo (dynamically linked with libu-boot.so)
$(OUTDIR)/demo: $(if $(demo-objs),$(addprefix $(OUTDIR)/,$(demo-objs)),$(DEMO_SRC))
	$(if $(demo-objs),$(CC) $(CFLAGS) -o $@ $^ $(SHARED_LDFLAGS),$(CC) $(CFLAGS) $(SYSTEM_CFLAGS) -o $@ $< $(SHARED_LDFLAGS))

# Build demo_static (statically linked with libu-boot.a)
$(OUTDIR)/demo_static: $(if $(demo-objs),$(addprefix $(OUTDIR)/,$(demo-objs)),$(DEMO_SRC))
	$(if $(demo-objs),$(CC) $(CFLAGS) -o $@ $^ $(STATIC_LDFLAGS),$(CC) $(CFLAGS) $(SYSTEM_CFLAGS) -o $@ $< $(STATIC_LDFLAGS))

clean:
	$(Q)rm -f $(DEMO_BINS)

.PHONY: all clean
