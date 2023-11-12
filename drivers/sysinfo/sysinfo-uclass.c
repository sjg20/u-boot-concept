// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2017
 * Mario Six,  Guntermann & Drunck GmbH, mario.six@gdsys.cc
 */

#define LOG_CATEGORY UCLASS_SYSINFO

#include <common.h>
#include <dm.h>
#include <sysinfo.h>

struct sysinfo_priv {
	bool detected;
};

int sysinfo_get(struct udevice **devp)
{
	int ret = uclass_first_device_err(UCLASS_SYSINFO, devp);

	/*
	 * There is some very dodgy error handling in gazerbeam,
	 * do not return a device on error.
	 */
	if (ret)
		*devp = NULL;
	return ret;
}

int sysinfo_detect(struct udevice *dev)
{
	int ret;
	struct sysinfo_priv *priv = dev_get_uclass_priv(dev);
	struct sysinfo_ops *ops = sysinfo_get_ops(dev);

	if (!ops->detect)
		return -ENOSYS;

	ret = ops->detect(dev);
	if (!ret)
		priv->detected = true;

	return ret;
}

int sysinfo_get_fit_loadable(struct udevice *dev, int index, const char *type,
			     const char **strp)
{
	struct sysinfo_priv *priv = dev_get_uclass_priv(dev);
	struct sysinfo_ops *ops = sysinfo_get_ops(dev);

	if (!priv->detected)
		return -EPERM;

	if (!ops->get_fit_loadable)
		return -ENOSYS;

	return ops->get_fit_loadable(dev, index, type, strp);
}

int sysinfo_get_bool(struct udevice *dev, int id, bool *val)
{
	struct sysinfo_priv *priv = dev_get_uclass_priv(dev);
	struct sysinfo_ops *ops = sysinfo_get_ops(dev);

	if (!priv->detected)
		return -EPERM;

	if (!ops->get_bool)
		return -ENOSYS;

	return ops->get_bool(dev, id, val);
}

int sysinfo_get_int(struct udevice *dev, int id, int *val)
{
	struct sysinfo_priv *priv = dev_get_uclass_priv(dev);
	struct sysinfo_ops *ops = sysinfo_get_ops(dev);

	if (!priv->detected)
		return -EPERM;

	if (!ops->get_int)
		return -ENOSYS;

	return ops->get_int(dev, id, val);
}

int sysinfo_get_str(struct udevice *dev, int id, size_t size, char *val)
{
	struct sysinfo_priv *priv = dev_get_uclass_priv(dev);
	struct sysinfo_ops *ops = sysinfo_get_ops(dev);

	if (!priv->detected)
		return -EPERM;

	if (!ops->get_str)
		return -ENOSYS;

	return ops->get_str(dev, id, size, val);
}

static int sysinfo_post_probe(struct udevice *dev)
{
	const u32 *list;
	int size;

	list = dev_read_prop(dev, "probe-devices", &size);
	if (list) {
		int i;

		size /= sizeof(u32);
		for (i = 0; i < size; i++) {
			uint phandle = fdt32_to_cpu(list[i]);
			struct udevice *found;
			ofnode node;
			int ret;

			node = ofnode_get_by_phandle(phandle);
			if (!ofnode_valid(node)) {
				/* the node may have been dropped from SPL */
				log_debug("Cannot find device for phandle %d\n",
					  phandle);
				continue;
			}
			ret = device_get_global_by_ofnode(node, &found);
			if (ret) {
				/* the node may be missing a bootph,xxx tag */
				log_debug("Cannot find device for node '%s'\n",
					  ofnode_get_name(node));
				continue;
			}
		}
	}

	return 0;
}

UCLASS_DRIVER(sysinfo) = {
	.id		= UCLASS_SYSINFO,
	.name		= "sysinfo",
	.post_bind	= dm_scan_fdt_dev,
	.post_probe	= sysinfo_post_probe,
	.per_device_auto	= sizeof(bool),
};
