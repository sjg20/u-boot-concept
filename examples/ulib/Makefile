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

# Include configuration and helper functions
include config.mk

# Programs to build
progs := demo

# Program definitions - list of object files for each program
demo_objs := demo.o demo_helper.o

# Objects that need system headers (default is U-Boot headers)
sys-objs := demo_helper.o

# Include build rules (must come after variable definitions)
include rules.mk

clean:
	$(Q)rm -f $(all_bins) $(OUTDIR)/*.o

.PHONY: all clean
