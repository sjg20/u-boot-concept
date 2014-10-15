/*
 * Copyright (c) 2014 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _post_h
#define _post_h

#define POST_PORT		0x80

#define POST_START16		0x1f
#define POST_START		0x2f
#define POST_DEAD_CODE		0xee

#define POST_RESET_VECTOR_CORRECT	0x40
#define POST_ENTER_PROTECTED_MODE	0x41

/* Output a post code using al - value must be 0 to 0xff */
#ifdef __ASSEMBLY__
#define post_code(value) \
	movb	$value, %al; \
	outb	%al, $POST_PORT
#else
static inline void post_code(int code)
{
	outb(code, POST_PORT);
}
#endif

#endif
