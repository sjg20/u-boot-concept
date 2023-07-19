// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/*
 * Misc boot support
 */
#include <common.h>
#include <command.h>
#include <dm.h>
#include <log.h>
#include <net.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <power/pmic.h>
#include <linux/delay.h>

#ifdef CONFIG_CMD_GO

/* Allow ports to override the default behavior */
__attribute__((weak))
unsigned long do_go_exec(ulong (*entry)(int, char * const []), int argc,
				 char *const argv[])
{
	return entry (argc, argv);
}

static int do_go(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	ulong	addr, rc;
	int     rcode = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	addr = hextoul(argv[1], NULL);

	printf ("## Starting application at 0x%08lX ...\n", addr);
	flush();

	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */
	rc = do_go_exec ((void *)addr, argc - 1, argv + 1);
	if (rc != 0) rcode = 1;

	printf ("## Application terminated, rc = 0x%lX\n", rc);
	return rcode;
}

/* -------------------------------------------------------------------- */

U_BOOT_CMD(
	go, CONFIG_SYS_MAXARGS, 1,	do_go,
	"start application at address 'addr'",
	"addr [arg ...]\n    - start application at address 'addr'\n"
	"      passing 'arg' as arguments"
);

#endif

U_BOOT_CMD(
	reset, 2, 0,	do_reset,
	"Perform RESET of the CPU",
	"- cold boot without level specifier\n"
	"reset -w - warm reset if implemented"
);

#ifdef CONFIG_CMD_POWEROFF
#ifdef CONFIG_CMD_PMIC_POWEROFF
int do_poweroff(struct cmd_tbl *cmdtp, int flag,
		int argc, char *const argv[])
{
	struct uc_pmic_priv *pmic_priv;
	struct udevice *dev;
	int ret;

	for (uclass_find_first_device(UCLASS_PMIC, &dev);
	     dev;
	     uclass_find_next_device(&dev)) {
		if (dev && !device_active(dev)) {
			ret = device_probe(dev);
			if (ret)
				return ret;
		}

		/* value we need to check is set after probe */
		pmic_priv = dev_get_uclass_priv(dev);
		if (pmic_priv->sys_pow_ctrl) {
			ret = pmic_poweroff(dev);

			/* wait some time and then print error */
			mdelay(5000);
			log_err("Failed to power off!!!\n");
			return ret;
		}
	}

	/* no device should reach here */
	return 1;
}
#endif

U_BOOT_CMD(
	poweroff, 1, 0,	do_poweroff,
	"Perform POWEROFF of the device",
	""
);
#endif
