/*
 * Copyright (c) 2012, The Chromium Authors
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#define DEBUG

#include <common.h>
#include <malloc.h>
#include <asm/errno.h>

#define SIZE		4096
#define TEST_VAL	0xff

#ifdef CONFIG_USE_ARCH_MEMSET
# define _memset_name	memset
#else
# define _memset_name	arch_memset_for_test
extern void *arch_memset_for_test(void *, int, __kernel_size_t);
#endif

static void test_clear(uint8_t *buf, int size)
{
	while (size-- > 0)
		*buf++ = '\0';
}

static inline int check_val(uint8_t *buf, int i, int val)
{
	if (buf[i] != val) {
		printf("Expected %d at %d, got %d\n", val, i, buf[i]);
		return -1;
	}

	return 0;
}

static int test_check_memset(uint8_t *buf, int zero_upto, int test_val,
			     int test_val_upto, int size)
{
	int i;

	for (i = 0; i < zero_upto; i++) {
		if (check_val(buf, i, 0))
			return -1;
	}

	for (; i < test_val_upto; i++) {
		if (check_val(buf, i, test_val))
			return -1;
	}

	for (; i < size; i++) {
		if (check_val(buf, i, 0))
			return -1;
	}

	return 0;
}

static int test_memset(void *buf, int size)
{
	int offset;
	int i;

	for (offset = 0; offset < 64; offset++) {
		for (i = 0; i < 256; i++) {
			test_clear(buf, size);
			_memset_name(buf + offset, TEST_VAL, i);
			if (test_check_memset(buf, offset, TEST_VAL,
					      offset + i, SIZE)) {
				printf("Failed on offset %d, iteration %d\n",
				       offset, i);
				return -1;
			}
		}
	}

	return 0;
}

static int do_ut_string(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	void *buf;
	int ret;

	buf = memalign(SIZE, 64);
	if (!buf)
		return -ENOMEM;

	ret = test_memset(buf, SIZE);

	free(buf);

	printf("Test %s, ret=%d\n", ret ? "FAILED" : "PASSED", ret);

	return ret ? 1 : 0;
}

U_BOOT_CMD(
	ut_string,	5,	1,	do_ut_string,
	"Very basic test of string functions (currently only memset())",
	""
);
