#
# (C) Copyright 2002-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# SPDX-License-Identifier:	GPL-2.0+
#

ifeq ($(CONFIG_X86_64),)
obj-$(CONFIG_ARCH_EFI) += crt0-efi-ia32.o reloc_ia32.o
else
obj-$(CONFIG_ARCH_EFI) += crt0-efi-ia64.o reloc_ia64.o
endif

ifneq ($(CONFIG_EFI_STUB),)
CFLAGS_REMOVE_reloc_ia32.o += -mregparm=3
CFLAGS_reloc_ia32.o += -fpic -fshort-wchar

CFLAGS_REMOVE_reloc_x86_64.o += -mregparm=3 -march=i386 -m32
CFLAGS_reloc_x86_64.o += -fpic -fshort-wchar

AFLAGS_REMOVE_crt0-efi-x86_64.o += -mregparm=3 -march=i386 -m32
AFLAGS_crt0-efi-x86_64.o += -fpic -fshort-wchar

extra-$(CONFIG_EFI_STUB_32BIT) += crt0-efi-ia32.o reloc_ia32.o
extra-$(CONFIG_EFI_STUB_64BIT) += crt0-efi-x86_64.o reloc_x86_64.o
endif

obj-y += bios.o
obj-y += bios_asm.o
obj-y += bios_interrupts.o
obj-$(CONFIG_CMD_BOOTM) += bootm.o
obj-y	+= cmd_boot.o
obj-$(CONFIG_HAVE_FSP) += cmd_hob.o
obj-y	+= gcc.o
obj-y	+= init_helpers.o
obj-y	+= interrupts.o
obj-y	+= lpc-uclass.o
obj-y	+= mpspec.o
obj-y += cmd_mtrr.o
obj-$(CONFIG_SYS_PCAT_INTERRUPTS) += pcat_interrupts.o
obj-$(CONFIG_SYS_PCAT_TIMER) += pcat_timer.o
ifndef CONFIG_DM_PCI
obj-$(CONFIG_PCI) += pci_type1.o
endif
obj-y	+= pch-uclass.o
obj-y	+= pirq_routing.o
obj-y	+= relocate.o
obj-y += physmem.o
obj-$(CONFIG_X86_RAMTEST) += ramtest.o
obj-y += sfi.o
obj-y	+= string.o
obj-y	+= tables.o
obj-$(CONFIG_SYS_X86_TSC_TIMER)	+= tsc_timer.o
obj-$(CONFIG_CMD_ZBOOT)	+= zimage.o
obj-$(CONFIG_HAVE_FSP) += fsp/

extra-$(CONFIG_USE_PRIVATE_LIBGCC) += lib.a

NORMAL_LIBGCC = $(shell $(CC) $(PLATFORM_CPPFLAGS) -print-libgcc-file-name)
OBJCOPYFLAGS := --prefix-symbols=__normal_
$(obj)/lib.a: $(NORMAL_LIBGCC) FORCE
	$(call if_changed,objcopy)
