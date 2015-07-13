#
# (C) Copyright 2015 Google, Inc
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-$(CONFIG_ARCH_EFI) += efi.o

CFLAGS_REMOVE_efi_stub.o := -mregparm=3 \
	$(if $(CONFIG_EFI_STUB_64BIT),-march=i386 -m32)
CFLAGS_efi_stub.o := -fpic -fshort-wchar

extra-$(CONFIG_EFI_STUB) += efi_stub.o
