# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2002
# Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
PLATFORM_RELFLAGS += $(call cc-option,-mbranch-protection=none)

PF_NO_UNALIGNED := $(call cc-option, -mstrict-align)
PLATFORM_CPPFLAGS += $(PF_NO_UNALIGNED)

EFI_LDS := elf_aarch64_efi.lds
EFI_CRT0 := crt0_aarch64_efi.o
EFI_RELOC := reloc_aarch64_efi.o

LDSCRIPT_EFI := $(srctree)/arch/arm/lib/elf_aarch64_efi.lds
EFISTUB := crt0_aarch64_efi.o reloc_aarch64_efi.o
#pei-aarch64-littleOBJCOPYFLAGS_EFI += -O binary  #--target=efi-app-aarch64
#--target=pei-aarch64-little
EFIPAYLOAD_BFDTARGET := pei-aarch64-little
EFIPAYLOAD_BFDARCH := aarch64
LDFLAGS_EFI_PAYLOAD := -Bsymbolic -Bsymbolic-functions -shared --no-undefined \
		       -s -zexecstack

AFLAGS_REMOVE_crt0-efi-aarch64.o += $(CFLAGS_NON_EFI)
AFLAGS_crt0-efi-aarch64.o += -fPIC

ifeq ($(CONFIG_EFI_APP),y)

KBUILD_LDFLAGS :=
PLATFORM_CPPFLAGS += $(CFLAGS_EFI)
LDFLAGS_FINAL += -Bsymbolic  -shared --no-undefined
LDFLAGS_FINAL += -zexecstack -znocombreloc
LDFLAGS_FINAL += -Bsymbolic-functions
LDFLAGS_FINAL += -z common-page-size=4096 -z max-page-size=4096
LDFLAGS_FINAL += --warn-common
LDSCRIPT := $(LDSCRIPT_EFI)

LDFLAGS_FINAL +=

endif
