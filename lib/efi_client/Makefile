# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2015 Google, Inc

obj-$(CONFIG_EFI_APP) += efi_app.o efi.o efi_app_init.o efi_dtb.o
#obj-$(CONFIG_EFI_APP) += efi_vars.o
obj-$(CONFIG_EFI_APP) += sdram.o

ifndef CONFIG_EFI_LOADER
obj-$(CONFIG_EFI_APP) += app_run.o
endif

obj-y += run.o
obj-$(CONFIG_EFI_STUB) += efi_info.o

ifeq ($(CONFIG_ARM64),y)
stub_obj := stub_arm64.o
else
stub_obj := stub_x86.o

ifeq ($(CONFIG_EFI_STUB_64BIT),y) # && !CONFIG_ARM64
CFLAGS_REMOVE_$(stub_obj) := -march=i386 -m32
CFLAGS_$(stub_obj) := -m64
CFLAGS_REMOVE_stub.o := -march=i386 -m32
CFLAGS_stub.o := -m64
CFLAGS_REMOVE_efi.o := -march=i386 -m32
CFLAGS_efi.o := -fpic -m64
endif
endif

CFLAGS_REMOVE_$(stub_obj) += -mregparm=3
CFLAGS_$(stub_obj) += -fpic -fshort-wchar
CFLAGS_REMOVE_stub.o += -mregparm=3
CFLAGS_stub.o += -fpic -fshort-wchar
CFLAGS_REMOVE_efi.o += -mregparm=3
CFLAGS_efi.o += -fpic -fshort-wchar

extra-$(CONFIG_EFI_STUB) += $(stub_obj) stub.o efi.o
