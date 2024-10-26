// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <efi_log.h>
#include <test/lib.h>
#include <test/test.h>
#include <test/ut.h>

static int lib_test_efi_log(struct unit_test_state *uts)
{
	void *buf;
	u64 addr;
	int ofs1, ofs2;

	// ut_assertok(efi_log_init());
	ut_assertok(efi_log_reset());

	ofs1 = efi_logs_allocate_pool(EFI_BOOT_SERVICES_DATA, 100, &buf);
	ofs2 = efi_logs_allocate_pages(EFI_ALLOCATE_ANY_PAGES,
				       EFI_BOOT_SERVICES_CODE, 10, &addr);
	ut_assertok(efi_loge_allocate_pages(ofs2, EFI_LOAD_ERROR, &addr));
	ut_assertok(efi_loge_allocate_pool(ofs1, 0, &buf));

	ut_assertok(efi_log_show());

	return 0;
}
LIB_TEST(lib_test_efi_log, UTF_CONSOLE);
