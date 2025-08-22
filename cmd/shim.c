// SPDX-License-Identifier: GPL-2.0+
/*
 * Control of settings for Shim
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define __efi_runtime

#include <abuf.h>
#include <command.h>
#include <efi.h>
#include <efi_variable.h>
#include <errno.h>
#include <shim.h>
#include <vsprintf.h>

static int do_shim_debug(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	efi_status_t eret;
	struct abuf buf;
	const char *sub;
	u32 value;
	int ret;

	sub = cmd_arg1(argc, argv);
	if (!sub) {
		ret = efi_read_var(SHIM_VERBOSE_VAR_NAME, &efi_shim_lock, NULL,
				   &buf, NULL);
		if (ret == -ENOENT) {
			value = 0;
		} else if (ret) {
			printf("Failed to read variable (err=%dE)\n", ret);
			goto fail;
		} else if (buf.size != sizeof(value)) {
			printf("Invalid value size %zd\n", buf.size);
			goto fail;
		} else {
			value = *(typeof(value) *)buf.data;
		}
		printf("%d\n", value);
	} else {
		value = hextoul(sub, NULL) ? 1 : 0;
		eret = efi_set_variable_int(SHIM_VERBOSE_VAR_NAME,
					    &efi_shim_lock,
					    EFI_VARIABLE_BOOTSERVICE_ACCESS,
					    sizeof(value), &value, false);
		if (eret) {
			printf("Failed to write variable (err=%lx)\n", eret);
			goto fail;
		}
	}

	return 0;

fail:
	return 1;
}

U_BOOT_LONGHELP(shim,
	"debug [<0/1>]  - Enable / disable debug verbose mode");

U_BOOT_CMD_WITH_SUBCMDS(shim, "Shim utilities", shim_help_text,
	U_BOOT_SUBCMD_MKENT(debug, 3, 1, do_shim_debug));
