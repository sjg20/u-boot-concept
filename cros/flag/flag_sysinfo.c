// SPDX-License-Identifier: GPL-2.0+
/*
 * A vboot flag controlled by coreboot sysinfo tables
 *
 * Copyright (c) 2018 Google, Inc
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>

/* Bring this in manually so that sandbox can compile this file */
#include <../arch/x86/include/asm/coreboot_tables.h>
#include <../arch/x86/include/asm/arch-coreboot/sysinfo.h>

struct flag_sysinfo_priv {
	const char *name;
	int port;
	bool active_high;
	int value;
};

static int flag_sysinfo_read(struct udevice *dev)
{
	struct flag_sysinfo_priv *priv = dev_get_priv(dev);

	return priv->value;
}

static int flag_sysinfo_probe(struct udevice *dev)
{
	struct flag_sysinfo_priv *priv = dev_get_priv(dev);
	struct sysinfo_t *sysinfo;
	const char *name;
	int i;

	name = dev_read_string(dev, "google,name");
	if (!name) {
		vboot_log(LOGL_ERR, "Missing flag name in '%s'\n",
			  dev->name);
		return -EINVAL;
	}
	priv->name = name;
	sysinfo = lib_sysinfo_get();
	for (i = 0; i < sysinfo->num_gpios; i++) {
		if (strncmp((char *)sysinfo->gpios[i].name, name,
						GPIO_MAX_NAME_LENGTH))
			continue;

		/* Entry found */
		priv->port = sysinfo->gpios[i].port;
		priv->active_high = sysinfo->gpios[i].polarity;
		priv->value = sysinfo->gpios[i].value;
		if (!priv->active_high)
			priv->value = !priv->value;

		return 0;
	}

	return 0;
}

static const struct vboot_flag_ops flag_sysinfo_ops = {
	.read	= flag_sysinfo_read,
};

static const struct udevice_id flag_sysinfo_ids[] = {
	{ .compatible = "google,sysinfo-flag" },
	{ }
};

U_BOOT_DRIVER(flag_sysinfo_drv) = {
	.name		= "flag_sysinfo",
	.id		= UCLASS_CROS_VBOOT_FLAG,
	.of_match	= flag_sysinfo_ids,
	.probe		= flag_sysinfo_probe,
	.ops		= &flag_sysinfo_ops,
	.priv_auto_alloc_size	= sizeof(struct flag_sysinfo_priv),
};
