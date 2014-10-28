/*
 * From Coreboot file device/oprom/realmode/x86.c
 *
 * Copyright (C) 2007 Advanced Micro Devices, Inc.
 * Copyright (C) 2009-2010 coresystems GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0
 */
#define DEBUG
#define CONFIG_REALMODE_DEBUG

#include <common.h>
#include <asm/i8259.h>
#include <asm/io.h>
#include <asm/post.h>
#include <asm/processor.h>
#include <asm/vbe.h>
#include "bios.h"
#include "../../../drivers/bios_emulator/include/x86emu/regs.h"

/* The following symbols cannot be used directly. They need to be fixed up
 * to point to the correct address location after the code has been copied
 * to REALMODE_BASE. Absolute symbols are not used because those symbols are
 * relocated when a relocatable ramstage is enabled.
 */
extern unsigned char __realmode_call, __realmode_interrupt;
extern unsigned char __realmode_buffer;

#define PTR_TO_REAL_MODE(sym)\
	(void *)(REALMODE_BASE + ((char *)&(sym) - (char *)&__realmode_code))

/* to have a common register file for interrupt handlers */
X86EMU_sysEnv _X86EMU_env;

void (*realmode_call)(u32 addr, u32 eax, u32 ebx, u32 ecx, u32 edx,
		u32 esi, u32 edi) asmlinkage;

void (*realmode_interrupt)(u32 intno, u32 eax, u32 ebx, u32 ecx, u32 edx,
		u32 esi, u32 edi) asmlinkage;

static void setup_realmode_code(void)
{
	memcpy(REALMODE_BASE, &__realmode_code, __realmode_code_size);

	/* Ensure the global pointers are relocated properly. */
	realmode_call = PTR_TO_REAL_MODE(__realmode_call);
	realmode_interrupt = PTR_TO_REAL_MODE(__realmode_interrupt);

	debug("Real mode stub @%p: %d bytes\n", REALMODE_BASE,
			__realmode_code_size);
}

static void setup_rombios(void)
{
	const char date[] = "06/11/99";
	memcpy((void *)0xffff5, &date, 8);

	const char ident[] = "PCI_ISA";
	memcpy((void *)0xfffd9, &ident, 7);

	/* system model: IBM-AT */
	writeb(0xfc, 0xffffe);
}

static int (*intXX_handler[256])(void) = { NULL };

static int intXX_exception_handler(void)
{
	/* compatibility shim */
	struct eregs reg_info = {
		.eax=X86_EAX,
		.ecx=X86_ECX,
		.edx=X86_EDX,
		.ebx=X86_EBX,
		.esp=X86_ESP,
		.ebp=X86_EBP,
		.esi=X86_ESI,
		.edi=X86_EDI,
		.vector=M.x86.intno,
		.error_code=0, // FIXME: fill in
		.eip=X86_EIP,
		.cs=X86_CS,
		.eflags=X86_EFLAGS
	};
	struct eregs *regs = &reg_info;

	debug("Oops, exception %d while executing option rom\n",
			regs->vector);
	cpu_hlt();

	return 0;		// Never really returns
}

static int intXX_unknown_handler(void)
{
	debug("Unsupported software interrupt #0x%x eax 0x%x\n",
			M.x86.intno, X86_EAX);

	return -1;
}

/* setup interrupt handlers for mainboard */
void mainboard_interrupt_handlers(int intXX, int (*intXX_func)(void))
{
	intXX_handler[intXX] = intXX_func;
}

static void setup_interrupt_handlers(void)
{
	int i;

	/* The first 16 intXX functions are not BIOS services,
	 * but the CPU-generated exceptions ("hardware interrupts")
	 */
	for (i = 0; i < 0x10; i++)
		intXX_handler[i] = &intXX_exception_handler;

	/* Mark all other intXX calls as unknown first */
	for (i = 0x10; i < 0x100; i++)
	{
		/* If the mainboard_interrupt_handler isn't called first.
		 */
		if(!intXX_handler[i])
		{
			/* Now set the default functions that are actually
			 * needed to initialize the option roms. This is
			 * very slick, as it allows us to implement mainboard
			 * specific interrupt handlers, such as the int15.
			 */
			switch (i) {
			case 0x10:
				intXX_handler[0x10] = &int10_handler;
				break;
			case 0x12:
				intXX_handler[0x12] = &int12_handler;
				break;
			case 0x16:
				intXX_handler[0x16] = &int16_handler;
				break;
			case 0x1a:
				intXX_handler[0x1a] = &int1a_handler;
				break;
			default:
				intXX_handler[i] = &intXX_unknown_handler;
				break;
			}
		}
	}
}

static void write_idt_stub(void *target, u8 intnum)
{
	unsigned char *codeptr;

	codeptr = (unsigned char *)target;
	memcpy(codeptr, &__idt_handler, __idt_handler_size);
	codeptr[3] = intnum; /* modify int# in the code stub. */
}

static void setup_realmode_idt(void)
{
	struct realmode_idt *idts = (struct realmode_idt *) 0;
	int i;

	/* Copy IDT stub code for each interrupt. This might seem wasteful
	 * but it is really simple
	 */
	 for (i = 0; i < 256; i++) {
		idts[i].cs = 0;
		idts[i].offset = 0x1000 + (i * __idt_handler_size);
		write_idt_stub((void *)((u32 )idts[i].offset), i);
	}

	/* Many option ROMs use the hard coded interrupt entry points in the
	 * system bios. So install them at the known locations.
	 */

	/* int42 is the relocated int10 */
	write_idt_stub((void *)0xff065, 0x42);
	/* BIOS Int 11 Handler F000:F84D */
	write_idt_stub((void *)0xff84d, 0x11);
	/* BIOS Int 12 Handler F000:F841 */
	write_idt_stub((void *)0xff841, 0x12);
	/* BIOS Int 13 Handler F000:EC59 */
	write_idt_stub((void *)0xfec59, 0x13);
	/* BIOS Int 14 Handler F000:E739 */
	write_idt_stub((void *)0xfe739, 0x14);
	/* BIOS Int 15 Handler F000:F859 */
	write_idt_stub((void *)0xff859, 0x15);
	/* BIOS Int 16 Handler F000:E82E */
	write_idt_stub((void *)0xfe82e, 0x16);
	/* BIOS Int 17 Handler F000:EFD2 */
	write_idt_stub((void *)0xfefd2, 0x17);
	/* ROM BIOS Int 1A Handler F000:FE6E */
	write_idt_stub((void *)0xffe6e, 0x1a);
}

#if CONFIG_FRAMEBUFFER_SET_VESA_MODE
vbe_mode_info_t mode_info;
static int mode_info_valid;

int vbe_mode_info_valid(void)
{
	return mode_info_valid;
}

static u8 vbe_get_mode_info(vbe_mode_info_t * mi)
{
	debug("VBE: Getting information about VESA mode %04x\n",
		mi->video_mode);
	char *buffer = PTR_TO_REAL_MODE(__realmode_buffer);
	u16 buffer_seg = (((unsigned long)buffer) >> 4) & 0xff00;
	u16 buffer_adr = ((unsigned long)buffer) & 0xffff;
	realmode_interrupt(0x10, VESA_GET_MODE_INFO, 0x0000,
			mi->video_mode, 0x0000, buffer_seg, buffer_adr);
	memcpy(mi->mode_info_block, buffer, sizeof(vbe_mode_info_t));
	mode_info_valid = 1;
	return 0;
}

static u8 vbe_set_mode(vbe_mode_info_t * mi)
{
	debug("VBE: Setting VESA mode %04x\n", mi->video_mode);
	// request linear framebuffer mode
	mi->video_mode |= (1 << 14);
	// request clearing of framebuffer
	mi->video_mode &= ~(1 << 15);
	realmode_interrupt(0x10, VESA_SET_MODE, mi->video_mode,
			0x0000, 0x0000, 0x0000, 0x0000);
	return 0;
}

/* These two functions could probably even be generic between
 * yabel and x86 native. TBD later.
 */
void vbe_set_graphics(void)
{
	mode_info.video_mode = (1 << 14) | CONFIG_FRAMEBUFFER_VESA_MODE;
	vbe_get_mode_info(&mode_info);
	unsigned char *framebuffer =
		(unsigned char *)mode_info.vesa.phys_base_ptr;
	debug("VBE: resolution:  %dx%d@%d\n",
		le16_to_cpu(mode_info.vesa.x_resolution),
		le16_to_cpu(mode_info.vesa.y_resolution),
		mode_info.vesa.bits_per_pixel);
	debug("VBE: framebuffer: %p\n", framebuffer);
	if (!framebuffer) {
		debug("VBE: Mode does not support linear "
			"framebuffer\n");
		return;
	}

	vbe_set_mode(&mode_info);
}

void vbe_textmode_console(void)
{
	udelay(2);
	realmode_interrupt(0x10, 0x0003, 0x0000, 0x0000,
				0x0000, 0x0000, 0x0000);
}

#endif

void bios_run_on_x86(pci_dev_t pcidev, unsigned long addr)
{
	u32 num_dev;

	num_dev = PCI_BUS(pcidev) << 8 | PCI_DEV(pcidev) << 3 |
			PCI_FUNC(pcidev);

	/* Setting up required hardware.
	 * Removing this will cause random illegal instruction exceptions
	 * in some option roms.
	 */
	i8259_setup();

	/* Set up some legacy information in the F segment */
	setup_rombios();

	/* Set up C interrupt handlers */
	setup_interrupt_handlers();

	/* Set up real-mode IDT */
	setup_realmode_idt();

	/* Make sure the code is placed. */
	setup_realmode_code();

	disable_caches();
	debug("Calling Option ROM at %lx, pci device %#x...\n", addr, num_dev);
	/* TODO ES:DI Pointer to System BIOS PnP Installation Check Structure */
	/* Option ROM entry point is at OPROM start + 3 */
	post_code(0xd1);
	realmode_call(addr + 0x0003, num_dev, 0xffff, 0x0000, 0xffff, 0x0,
		      0x0);
	post_code(0xd2);
	debug("... Option ROM returned.\n");

#if CONFIG_FRAMEBUFFER_SET_VESA_MODE
	vbe_set_graphics();
#endif
}

/* interrupt_handler() is called from assembler code only,
 * so there is no use in putting the prototype into a header file.
 */
int asmlinkage interrupt_handler(u32 intnumber,
	    u32 gsfs, u32 dses,
	    u32 edi, u32 esi,
	    u32 ebp, u32 esp,
	    u32 ebx, u32 edx,
	    u32 ecx, u32 eax,
	    u32 cs_ip, u16 stackflags);

int asmlinkage interrupt_handler(u32 intnumber,
	    u32 gsfs, u32 dses,
	    u32 edi, u32 esi,
	    u32 ebp, u32 esp,
	    u32 ebx, u32 edx,
	    u32 ecx, u32 eax,
	    u32 cs_ip, u16 stackflags)
{
	u32 ip;
	u32 cs;
	u32 flags;
	int ret = 0;

	ip = cs_ip & 0xffff;
	cs = cs_ip >> 16;
	flags = stackflags;

#ifdef CONFIG_REALMODE_DEBUG
	debug("oprom: INT# 0x%x\n", intnumber);
	debug("oprom: eax: %08x ebx: %08x ecx: %08x edx: %08x\n",
		      eax, ebx, ecx, edx);
	debug("oprom: ebp: %08x esp: %08x edi: %08x esi: %08x\n",
		     ebp, esp, edi, esi);
	debug("oprom:  ip: %04x      cs: %04x   flags: %08x\n",
		     ip, cs, flags);
#endif

	// Fetch arguments from the stack and put them to a place
	// suitable for the interrupt handlers
	X86_EAX = eax;
	X86_ECX = ecx;
	X86_EDX = edx;
	X86_EBX = ebx;
	X86_ESP = esp;
	X86_EBP = ebp;
	X86_ESI = esi;
	X86_EDI = edi;
	M.x86.intno = intnumber;
	/* TODO: error_code must be stored somewhere */
	X86_EIP = ip;
	X86_CS = cs;
	X86_EFLAGS = flags;

	// Call the interrupt handler for this int#
	ret = intXX_handler[intnumber]();

	// Put registers back on the stack. The assembler code
	// will later pop them.
	// What happens here is that we force (volatile!) changing
	// the values of the parameters of this function. We do this
	// because we know that they stay alive on the stack after
	// we leave this function. Don't say this is bollocks.
	*(volatile u32 *)&eax = X86_EAX;
	*(volatile u32 *)&ecx = X86_ECX;
	*(volatile u32 *)&edx = X86_EDX;
	*(volatile u32 *)&ebx = X86_EBX;
	*(volatile u32 *)&esi = X86_ESI;
	*(volatile u32 *)&edi = X86_EDI;
	flags = X86_EFLAGS;

	/* Pass success or error back to our caller via the CARRY flag */
	if (ret) {
		flags &= ~1; // no error: clear carry
	}else{
		debug("int%02x call returned error.\n", intnumber);
		flags |= 1;  // error: set carry
	}
	*(volatile u16 *)&stackflags = flags;

	/* The assembler code doesn't actually care for the return value,
	 * but keep it around so its expectations are met */
	return ret;
}

