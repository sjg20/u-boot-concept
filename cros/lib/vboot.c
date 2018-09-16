/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <mapmem.h>
#include <asm/io.h>
#include <cros_ec.h>
#include <cros/power_management.h>
#include <cros/vboot.h>
#ifdef CONFIG_SYS_COREBOOT
#include <asm/arch/sysinfo.h>
#endif

// static struct vboot_info s_vboot;

int vboot_alloc(struct vboot_info **vbootp)
{
	gd->vboot = malloc(sizeof(struct vboot_info));
	if (!gd->vboot) {
		log(LOGC_VBOOT, LOGL_ERR, "Cannot allocate vboot %d\n",
		    sizeof(struct vboot_info));
		return -ENOMEM;
	}
	memset(gd->vboot, '\0', sizeof(struct vboot_info));
	*vbootp = gd->vboot;

	return 0;
}

struct vboot_info *vboot_get(void)
{
	struct vboot_info *vboot = gd->vboot;

	return vboot->valid ? vboot : NULL;
}

struct vboot_info *vboot_get_alloc(void)
{
	struct vboot_info *vboot = gd->vboot;

	if (!vboot)
		vboot_alloc(&vboot);

	return vboot;
}

struct vboot_info *vboot_get_nocheck(void)
{
	return gd->vboot;
}

int vboot_load_config(const void *blob, struct vboot_info *vboot)
{
	ofnode node;

	node = cros_ofnode_config_node();
	if (!ofnode_valid(node))
		return -ENOENT;

	return 0;
}

/* Request the EC reboot to RO when the AP shuts down. */
int vboot_request_ec_reboot_to_ro(void)
{
#ifdef CONFIG_CROS_EC
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_CROS_EC, &dev);
	if (ret) {
		VB2_DEBUG("%s: no cro_ec device: cannot request EC reboot to RO\n",
			  __func__);
		return ret;
	}

	return cros_ec_reboot(dev, EC_REBOOT_COLD,
			      EC_REBOOT_FLAG_ON_AP_SHUTDOWN);
#else
	return 0;
#endif
}

int vboot_set_error(struct vboot_info *vboot, const char *stage,
		    enum VbErrorPredefined_t err)
{
	VB2_DEBUG("Stage '%s' produced vboot error %#x\n", stage, err);
	vboot->vb_error = err;

	return -1;
}

void vboot_init_cparams(struct vboot_info *vboot, VbCommonParams *cparams)
{
#ifdef VBOOT_GBB_DATA
	cparams->gbb_data = vboot->gbb;
	cparams->gbb_size = vboot->fmap.readonly.gbb.length;
#endif
#ifdef CONFIG_SYS_COREBOOT
	cparams->shared_data_blob =
		&((chromeos_acpi_t *)lib_sysinfo.vdat_addr)->vdat;
	cparams->shared_data_size =
		sizeof(((chromeos_acpi_t *)lib_sysinfo.vdat_addr)->vdat);
#else
// 	cparams->shared_data_blob = vboot->vb_shared_data;
// 	cparams->shared_data_size = VB_SHARED_DATA_REC_SIZE;
#endif
// 	vboot->cparams.caller_context = vboot;
	VB2_DEBUG("cparams:\n");
#ifdef VBOOT_GBB_DATA
	VB2_DEBUG("- %-20s: %08x\n", "gbb_data",
		map_to_sysmem(cparams->gbb_data));
	VB2_DEBUG("- %-20s: %08x\n", "gbb_size", cparams->gbb_size);
#endif
	VB2_DEBUG("- %-20s: %08x\n", "shared_data_blob",
		(unsigned)map_to_sysmem(cparams->shared_data_blob));
	VB2_DEBUG("- %-20s: %08x\n", "shared_data_size",
		cparams->shared_data_size);
}

#ifdef CONFIG_VBOOT_REGION_READ
VbError_t VbExRegionRead(VbCommonParams *cparams,
			 enum vb_firmware_region region, uint32_t offset,
			 uint32_t size, void *buf)
{
	struct vboot_info *vboot = cparams->caller_context;
	fwstore_t *file = &vboot->file;

	if (region != VB_REGION_GBB) {
		VB2_DEBUG("Only GBB region is supported, region=%d\n", region);
		return VBERROR_REGION_READ_INVALID;
	}

// 	VB2_DEBUG("VbExRegionRead, offset=%x, size=%x\n",
// 		vboot->fmap.readonly.gbb.offset + offset, size);
	if (file->read(file, vboot->fmap.readonly.gbb.offset + offset, size,
		       buf)) {
		VB2_DEBUG("failed to read from gbb offset %x sze %x\n",
			offset, size);
		return VBERROR_REGION_READ_FAILED;
	}

	return 0;
}
#endif /* CONFIG_VBOOT_REGION_READ */

int vboot_is_slot_a(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);

	return !(ctx->flags & VB2_CONTEXT_FW_SLOT_B);
}

const char *vboot_slot_name(struct vboot_info *vboot)
{
	return vboot_is_slot_a(vboot) ? "A" : "B";
}

void vboot_set_selected_region(struct vboot_info *vboot,
			       const struct fmap_entry *spl,
			       const struct fmap_entry *u_boot)
{
	vboot->blob->spl_entry = *spl;
	vboot->blob->u_boot_entry = *u_boot;
}

bool vboot_config_bool(struct vboot_info *vboot, const char *prop)
{
	return ofnode_read_bool(vboot->config, prop);
}
