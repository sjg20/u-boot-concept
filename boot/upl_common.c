// SPDX-License-Identifier: GPL-2.0+
/*
 * UPL handoff command functions
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <cpu.h>
#include <dm.h>
#include <serial.h>
#include <string.h>
#include <upl.h>
#include <video.h>
#include <asm/global_data.h>
#include <dm/uclass-internal.h>

DECLARE_GLOBAL_DATA_PTR;

/* Names of bootmodes */
const char *const bootmode_names[UPLBM_COUNT] = {
	[UPLBM_FULL]	= "full",
	[UPLBM_MINIMAL]	= "minimal",
	[UPLBM_FAST]	= "fast",
	[UPLBM_DIAG]	= "diag",
	[UPLBM_DEFAULT]	= "default",
	[UPLBM_S2]	= "s2",
	[UPLBM_S3]	= "s3",
	[UPLBM_S4]	= "s4",
	[UPLBM_S5]	= "s5",
	[UPLBM_FACTORY]	= "factory",
	[UPLBM_FLASH]	= "flash",
	[UPLBM_RECOVERY] = "recovery",
};

/* Names of memory usages */
const char *const usage_names[UPLUS_COUNT] = {
	[UPLUS_ACPI_RECLAIM]	= "acpi-reclaim",
	[UPLUS_ACPI_NVS]	= "acpi-nvs",
	[UPLUS_BOOT_CODE]	= "boot-code",
	[UPLUS_BOOT_DATA]	= "boot-data",
	[UPLUS_RUNTIME_CODE]	= "runtime-code",
	[UPLUS_RUNTIME_DATA]	= "runtime-data",
};

/* Names of access types */
const char *const access_types[UPLUS_COUNT] = {
	[UPLAT_MMIO]	= "mmio",
	[UPLAT_IO]	= "io",
};

/* Names of graphics formats */
const char *const graphics_formats[UPLUS_COUNT] = {
	[UPLGF_ARGB32]	= "a8r8g8b8",
	[UPLGF_ABGR32]	= "a8b8g8r8",
	[UPLGF_ABGR64]	= "a16b16g16r16",
};

int upl_add_serial(struct upl_serial *ser)
{
	struct udevice *dev = gd->cur_serial_dev;
	struct serial_device_info info;
	u64 addr;
	int ret;

	if (!dev)
		return log_msg_ret("ser", -ENOENT);
	ret = serial_getinfo(dev, &info);
	if (ret)
		return log_msg_ret("inf", ret);

	ser->compatible = ofnode_read_string(dev_ofnode(dev), "compatible");
	ser->clock_frequency = info.clock;
	ser->current_speed = info.baudrate;

	/* Set bit 64 of the address if using I/O */
	addr = info.addr;
	if (info.addr_space == SERIAL_ADDRESS_SPACE_IO)
		addr |= BIT_ULL(32);
	ret = upl_add_region(&ser->reg, addr, info.size);

	ser->reg_io_shift = info.reg_shift;
	ser->reg_offset = info.reg_offset;
	ser->reg_io_width = info.reg_width;
	ser->virtual_reg = 0;
	ser->access_type = info.addr_space == SERIAL_ADDRESS_SPACE_IO ?
		UPLAT_IO : UPLAT_MMIO;

	return 0;
}

int upl_add_graphics(struct upl_graphics *gra, ulong *basep, ulong *sizep)
{
	struct video_uc_plat *plat;
	struct video_priv *priv;
	struct memregion region;
	struct udevice *dev;

	uclass_find_first_device(UCLASS_VIDEO, &dev);
	if (!dev || !device_active(dev))
		return log_msg_ret("vid", -ENOENT);

	plat = dev_get_uclass_plat(dev);
	region.base = plat->base;
	region.size = plat->size;
	*basep = plat->base;
	*sizep = plat->size;
	if (!alist_add(&gra->reg, region))
		return log_msg_ret("reg", -ENOMEM);

	priv = dev_get_uclass_priv(dev);
	gra->width = priv->xsize;
	gra->height = priv->ysize;
	gra->stride = priv->line_length;	/* private field */
	switch (priv->format) {
	case VIDEO_RGBA8888:
	case VIDEO_X8R8G8B8:
		gra->format = UPLGF_ARGB32;
		break;
	case VIDEO_X8B8G8R8:
		gra->format = UPLGF_ABGR32;
		break;
	case VIDEO_X2R10G10B10:
		log_debug("device '%s': VIDEO_X2R10G10B10 not supported\n",
			  dev->name);
		return log_msg_ret("for", -EPROTO);
	case VIDEO_UNKNOWN:
		log_debug("device '%s': Unknown video format\n", dev->name);
		return log_msg_ret("for", -EPROTO);
	}

	return 0;
}

int upl_create(struct upl *upl)
{
	struct upl_mem mem;
	ulong base, size;
	int ret;

	/* hard-code this for now to keep Tianocore happy */
	upl->addr_cells = 2;
	upl->size_cells = 1;

	upl->bootmode = 0;
	log_debug("conf_offset %d\n", upl->conf_offset);
	if (IS_ENABLED(CONFIG_X86))
		upl->addr_width =  cpu_phys_address_size();

	memset(&mem, '\0', sizeof(mem));
	alist_init_struct(&mem.region, struct memregion);

	ret = upl_add_region(&mem.region, gd->ram_base, gd->ram_size);
	if (ret)
		return log_msg_ret("uar", ret);
	if (!alist_add(&upl->mem, mem))
		return log_msg_ret("arg", -ENOMEM);

	ret = upl_add_serial(&upl->serial);
	if (ret && ret != -ENOENT)
		return log_msg_ret("ser", ret);
	ret = upl_add_graphics(&upl->graphics, &base, &size);
	if (ret && ret != -ENOENT)
		return log_msg_ret("gra", ret);

	return 0;
}

int upl_write_to_buf(struct upl *upl, ofnode root, struct abuf *buf)
{
	int ret;

	ret = upl_create(upl);
	if (ret)
		return log_msg_ret("uwr", ret);

	log_debug("writing to root node %d\n", ofnode_to_offset(root));
	ret = upl_write_handoff(upl, root, true);
	if (ret)
		return log_msg_ret("wr", ret);

	ret = oftree_to_fdt(oftree_default(), buf);
	if (ret)
		return log_msg_ret("fdt", ret);
	log_debug("FDT size %zx\n", abuf_size(buf));

	return 0;
}

int upl_add_region(struct alist *lst, u64 base, ulong size)
{
	struct memregion region;

	region.base = base;
	region.size = size;
	if (!alist_add(lst, region))
		return log_msg_ret("uar", -ENOMEM);

	return 0;
}

void upl_init(struct upl *upl)
{
	memset(upl, '\0', sizeof(struct upl));
	alist_init_struct(&upl->image, struct upl_image);
	alist_init_struct(&upl->mem, struct upl_mem);
	alist_init_struct(&upl->memres, struct upl_memres);
	alist_init_struct(&upl->serial.reg, struct memregion);
	alist_init_struct(&upl->graphics.reg, struct memregion);
}
