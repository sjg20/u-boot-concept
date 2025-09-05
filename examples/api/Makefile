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

# Build demo_static with linker script for proper linker list handling
demo_static: demo.c
	@mkdir -p $(UBOOT_BUILD)/examples/api
	gcc -o $(UBOOT_BUILD)/examples/api/$@ $< \
		-Wl,-T,$(LIB_STATIC_LDS) \
		-Wl,--whole-archive $(UBOOT_BUILD)/libu-boot.a -Wl,--no-whole-archive \
		-lSDL2 -lpthread -lm -ldl -static-libgcc

# Run targets
run-demo: demo
	LD_LIBRARY_PATH=$(UBOOT_BUILD) ./demo

run-demo-static: demo_static
	$(UBOOT_BUILD)/examples/api/demo_static

clean:
	rm -f $(PROGS) *.o $(UBOOT_BUILD)/examples/api/demo_static

.PHONY: all clean run-demo run-demo-static demo_static