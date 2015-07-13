#
# (C) Copyright 2000-2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# SPDX-License-Identifier:	GPL-2.0+
#

CONFIG_STANDALONE_LOAD_ADDR ?= 0x40000

PLATFORM_CPPFLAGS += -fno-strict-aliasing
PLATFORM_CPPFLAGS += -fomit-frame-pointer
PF_CPPFLAGS_X86   := $(call cc-option, -fno-toplevel-reorder, \
		       $(call cc-option, -fno-unit-at-a-time)) \
		     $(call cc-option, -mpreferred-stack-boundary=2)
ifeq ($(CONFIG_ARCH_EFI),y)
PF_CPPFLAGS_X86   += $(call cc-option, -mno-red-zone)
endif

PLATFORM_CPPFLAGS += $(PF_CPPFLAGS_X86)
PLATFORM_CPPFLAGS += -fno-dwarf2-cfi-asm
PLATFORM_CPPFLAGS += -march=i386 -m32

PLATFORM_RELFLAGS += -ffunction-sections -fvisibility=hidden

PLATFORM_LDFLAGS += -Bsymbolic -Bsymbolic-functions -m elf_i386

LDFLAGS_FINAL += --wrap=__divdi3 --wrap=__udivdi3
LDFLAGS_FINAL += --wrap=__moddi3 --wrap=__umoddi3

OBJCOPYFLAGS_EFI := -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	-j .rel -j .rela -j .reloc

ifeq ($(CONFIG_ARCH_EFI),y)

ifeq ($(ARCH),x86)
EFILIB=/usr/lib32
EFIARCH=ia32
else
EFILIB=/usr/lib
EFIARCH=x86_64
endif

PLATFORM_CPPFLAGS += -fpic -fshort-wchar
LDFLAGS_FINAL += -znocombreloc -shared
LDSCRIPT := $(srctree)/$(CPUDIR)/efi/elf_ia32_efi.lds
OBJCOPYFLAGS_EFI += --target=efi-app-ia32

else

PLATFORM_CPPFLAGS += -mregparm=3
PLATFORM_LDFLAGS += --emit-relocs
LDFLAGS_FINAL += --gc-sections -pie

endif
