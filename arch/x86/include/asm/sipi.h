/*
 * Copyright (c) 2015 Gooogle, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _ASM_SIPI_H
#define _ASM_SIPI_H

struct __packed sipi_params_16bit {
	u32 ap_start32;
	u16 segment;
	u16 pad;

	u16 gdt_limit;
	u32 gdt;
	u16 unused;
};

struct sipi_params {
	u32 flag;
	u32 idt_ptr;
	u32 ap_continue_addr;
	u32 stack_top;
	u32 stack_size;
	u32 microcode_lock;
	u32 microcode_ptr;
	u32 msr_table_ptr;
	u32 msr_count;
	u32 c_handler;
	atomic_t ap_count;
};

void ap_start(void);
void ap_start32(void);
void ap_continue(void);
void ap_code_end(void);

extern char sipi_params_16bit[];
extern char sipi_params[];

#endif
