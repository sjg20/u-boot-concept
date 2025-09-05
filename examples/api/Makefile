# SPDX-License-Identifier: GPL-2.0+
#
# Standalone Makefile for U-Boot library examples
#
# Copyright 2025 Canonical
# Written by Simon Glass <simon.glass@canonical.com>

# Paths - can be overridden by environment or command line
UBOOT_SRC ?= ../..
UBOOT_BUILD ?= /tmp/b/sandbox

# Programs to build
PROGS = demo

# Default target
all: $(PROGS)
	@mkdir -p $(UBOOT_BUILD)/examples/api
	@cp $(PROGS) $(UBOOT_BUILD)/examples/api/

# Build rules for each program
demo: demo.c
	gcc -o $@ $< -L$(UBOOT_BUILD) -lu-boot -Wl,-rpath,$(UBOOT_BUILD)

# Run targets
run-demo: demo
	LD_LIBRARY_PATH=$(UBOOT_BUILD) ./demo

clean:
	rm -f $(PROGS) *.o

.PHONY: all clean run-demo