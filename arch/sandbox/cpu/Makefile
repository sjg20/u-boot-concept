# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2011 The Chromium OS Authors.
#
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

obj-y	:= cache.o cpu.o state.o os.o
extra-y	:= start.o
extra-$(CONFIG_SANDBOX_SDL)    += sdl.o
obj-$(CONFIG_XPL_BUILD)	+= spl.o
obj-$(CONFIG_ETH_SANDBOX_RAW)	+= eth-raw-os.o

# Compile these files with system headers
CFLAGS_USE_SYSHDRS := eth-raw-os.o os.o sdl.o

# sdl.c fails to build with -fshort-wchar using musl
cmd_cc_sdl.o = $(CC) $(filter-out -nostdinc -fshort-wchar, \
	$(patsubst -I%,-idirafter%,$(c_flags))) -fno-lto -c -o $@ $<

$(obj)/sdl.o: $(src)/sdl.c FORCE
	$(call if_changed_dep,cc_sdl.o)
