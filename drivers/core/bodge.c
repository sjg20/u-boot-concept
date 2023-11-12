// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 Google LLC
 *
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <dm/root.h>
#include <dm/device-internal.h>

/**
 * dm_probe_devices() - Probe devices which want to be probed automatically
 *
 * Some devices want to be probed as soon as they are bound. Handle this by
 * checking the flag and probing as necessary.
 *
 * NOTE: there is almost no error checking on this process, so if a device failsss
 * to probe for any reason, it will be silently ignored
 *
 * @pre_reloc_only: If true, bind only nodes with special devicetree properties,
 * or drivers with the DM_FLAG_PRE_RELOC flag. If false bind all drivers.
 *
 * Return: 0 if OK, -ve it any default failed
 */
static int dm_probe_devices(struct udevice *dev, bool pre_reloc_only)
{
	ofnode node = dev_ofnode(dev);
	struct udevice *child;
	int ret;

	if (!pre_reloc_only ||
	    (ofnode_valid(node) && ofnode_pre_reloc(node)) ||
	    (dev->driver->flags & DM_FLAG_PRE_RELOC)) {
		if (dev_get_flags(dev) & DM_FLAG_PROBE_AFTER_BIND) {
			ret = device_probe(dev);
			if (ret)
				return ret;
		}
	}

	list_for_each_entry(child, &dev->child_head, sibling_node)
		dm_probe_devices(child, pre_reloc_only);

	return 0;
}

/*
 * dm_bodge_probe() - handle auto-probing of particular devices
 *
 * The code in here should be removed in favour of using a sysinfo driver to
 * probe required devices.
 *
 * See the probe-devices property or manually probe devices in the sysinfo
 * probe() method.
 *
 * For other users please send an email to the mailing list and cc the
 * driver model maintainer
 */
static int dm_bodge_probe(bool pre_reloc_only)
{
	int ret;

	ret = dm_probe_devices(gd->dm_root, pre_reloc_only);
	if (ret)
		return ret;

	return 0;
}

int dm_bodge(bool pre_reloc_only)
{
	int ret;

	if (IS_ENABLED(CONFIG_DM_BODGE_AUTO_PROBE)) {
		ret = dm_bodge_probe(pre_reloc_only);
		if (ret)
			return ret;
	}

	return 0;
}
