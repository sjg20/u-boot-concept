# SPDX-License-Identifier: GPL-2.0+
#
# Configuration and helpers for U-Boot library examples
#
# Copyright 2025 Canonical Ltd.
# Written by Simon Glass <simon.glass@canonical.com>

# For standalone builds, provide default values
EXAMPLE_DIR ?= .
OUTDIR ?= .
CC ?= gcc
SDL_CONFIG ?= sdl2-config
PLATFORM_LIBS ?= $(shell $(SDL_CONFIG) --libs)
LIB_STATIC_LDS ?= static.lds
# The main Makefile passes in Q=@ for quiet output
Q ?=

# Common compiler flags for programs using system headers
SYSTEM_CFLAGS := -I$(UBOOT_BUILD)/include \
	-idirafter$(srctree)/include \
	-include $(srctree)/include/linux/compiler_attributes.h

# Common compiler flags for programs using U-Boot headers (these match the
# U-Boot internal build)
UBOOT_CFLAGS := -nostdinc \
	-isystem $(shell $(CC) -print-file-name=include) \
	-I$(UBOOT_BUILD)/include \
	-I$(srctree)/include \
	-I$(srctree)/arch/sandbox/include \
	-include $(UBOOT_BUILD)/include/config.h \
	-include $(srctree)/include/linux/kconfig.h \
	-I$(srctree)/dts/upstream/include \
	"-DMBEDTLS_CONFIG_FILE=\"mbedtls_def_config.h\"" \
	-I$(srctree)/lib/mbedtls \
	-I$(srctree)/lib/mbedtls/port \
	-I$(srctree)/lib/mbedtls/external/mbedtls \
	-I$(srctree)/lib/mbedtls/external/mbedtls/include \
	-Wno-builtin-declaration-mismatch \
	-D__KERNEL__ -DCONFIG_SYS_TEXT_BASE=0

# Linking flags
SHARED_LDFLAGS := -L$(UBOOT_BUILD) -lu-boot -Wl,-rpath,$(UBOOT_BUILD)

STATIC_LDFLAGS := -Wl,-T,$(LIB_STATIC_LDS) \
	-Wl,--whole-archive $(UBOOT_BUILD)/libu-boot.a \
	-Wl,--no-whole-archive \
	-lpthread -ldl $(PLATFORM_LIBS) -Wl,-z,noexecstack
