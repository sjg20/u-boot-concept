// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for QEMU qfw
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <abuf.h>
#include <command.h>
#include <bootdev.h>
#include <bootflow.h>
#include <bootm.h>
#include <bootmeth.h>
#include <env.h>
#include <qfw.h>
#include <dm.h>

static int qfw_check(struct udevice *dev, struct bootflow_iter *iter)
{
	const struct udevice *media = dev_get_parent(iter->dev);
	enum uclass_id id = device_get_uclass_id(media);

	log_debug("media=%s\n", media->name);
	if (id == UCLASS_QFW)
		return 0;

	return -ENOTSUPP;
}

static int qfw_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	struct udevice *qfw_dev = dev_get_parent(bflow->dev);
	struct abuf kern, initrd;
	int ret;

	abuf_init_const_addr(&kern, env_get_hex("kernel_addr_r", 0), 0);
	abuf_init_const_addr(&initrd, env_get_hex("ramdisk_addr_r", 0), 0);
	log_debug("setup kernel %s %lx %lx\n", qfw_dev->name, abuf_addr(&kern),
		  abuf_addr(&initrd));
	bflow->name = strdup("qfw");
	if (!bflow->name)
		return log_msg_ret("name", -ENOMEM);

	ret = qemu_fwcfg_setup_kernel(qfw_dev, &kern, &initrd);
	log_debug("setup kernel result %d\n", ret);
	if (ret)
		return log_msg_ret("cmd", -EIO);

	if (!bootflow_img_add(bflow, "qfw-kern",
			      (enum bootflow_img_t)IH_TYPE_KERNEL,
			      abuf_addr(&kern), kern.size))
		return log_msg_ret("qke", -ENOMEM);

	if (initrd.size &&
	    !bootflow_img_add(bflow, "qfw-initrd",
			      (enum bootflow_img_t)IH_TYPE_RAMDISK,
			      abuf_addr(&initrd), initrd.size))
		return log_msg_ret("qrd", -ENOMEM);

	bflow->state = BOOTFLOWST_READY;

	return 0;
}

static int qfw_read_file(struct udevice *dev, struct bootflow *bflow,
			 const char *file_path, ulong addr,
			 enum bootflow_img_t type, ulong *sizep)
{
	int qemu_fwcfg_setup_kernel(struct udevice *qfw_dev, struct abuf *kern,
			    struct abuf *initrd)

	return -ENOSYS;
}

static int qfw_boot(struct udevice *dev, struct bootflow *bflow)
{
	const struct bootflow_img *kern, *initrd;
	int ret;

	kern = bootflow_img_find(bflow, (enum bootflow_img_t)IH_TYPE_KERNEL);
	initrd = bootflow_img_find(bflow, (enum bootflow_img_t)IH_TYPE_RAMDISK);
	if (!kern)
		return log_msg_ret("qbk", -ENOENT);

	ret = run_command("booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdtcontroladdr}",
			  0);
	if (ret) {
		ret = run_command("bootz ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} "
				  "${fdtcontroladdr}", 0);
	}
	if (ret) {
		ret = zboot_run_args(kern->addr, kern->size,
			initrd->addr, initrd->size, 0, NULL);
	}

	return ret ? -EIO : 0;
}

static int qfw_bootmeth_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = "QEMU boot using firmware interface";

	return 0;
}

static struct bootmeth_ops qfw_bootmeth_ops = {
	.check		= qfw_check,
	.read_bootflow	= qfw_read_bootflow,
	.read_file	= qfw_read_file,
	.boot		= qfw_boot,
};

static const struct udevice_id qfw_bootmeth_ids[] = {
	{ .compatible = "u-boot,qfw-bootmeth" },
	{ }
};

U_BOOT_DRIVER(bootmeth_qfw) = {
	.name		= "bootmeth_qfw",
	.id		= UCLASS_BOOTMETH,
	.of_match	= qfw_bootmeth_ids,
	.ops		= &qfw_bootmeth_ops,
	.bind		= qfw_bootmeth_bind,
};
