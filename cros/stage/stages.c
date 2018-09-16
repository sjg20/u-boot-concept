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
#include <errno.h>
#include <spl.h>
#include <cros/antirollback.h>
#include <cros/nvdata.h>
#include <cros/power_management.h>
#include <cros/stages.h>
#include <cros/vboot.h>

struct vboot_stage {
	const char *name;
	int (*run)(struct vboot_info *vboot);
};

/* There are three groups here. TPL runs the early firmware-selection process,
 * then SPL sets up SDRAM and jumps to U-Boot proper, which does the kernel-
 * selection process. We only build in the code that is actually needed by each
 * stage.
 */
struct vboot_stage stages[] = {
#ifdef CONFIG_TPL_BUILD
	/* Verification stage: figures out which firmware to run */
	[VBOOT_STAGE_VER_INIT] = {"ver_init", vboot_ver_init},
	[VBOOT_STAGE_VER1_VBINIT] = {"ver1_vbinit", vboot_ver1_vbinit},
	[VBOOT_STAGE_VER2_SELECTFW] = {"ver2_selectfw", vboot_ver2_select_fw,},
	[VBOOT_STAGE_VER3_TRYFW] = {"ver3_tryfw", vboot_ver3_try_fw,},
	[VBOOT_STAGE_VER4_LOCATEFW] = {"ver4_locatefw", vboot_ver4_locate_fw,},
	[VBOOT_STAGE_VER_FINISH] = {"ver5_finishfw", vboot_ver5_finish_fw,},
	[VBOOT_STAGE_VER_JUMP] = {"ver_jump", vboot_ver6_jump_fw,},
#elif defined(CONFIG_SPL_BUILD)
	/* SPL stage: Sets up SDRAM and jumps to U-Boot proper */
	[VBOOT_STAGE_SPL_INIT] = {"spl_init", vboot_spl_init,},
	[VBOOT_STAGE_SPL_JUMP_U_BOOT] =
		{"spl_jump_u_boot", vboot_spl_jump_u_boot,},
	[VBOOT_STAGE_RW_INIT] = {},
#else
	/* U-Boot stage: Boots the kernel */
	[VBOOT_STAGE_RW_INIT] = {"rw_init", vboot_rw_init,},
	[VBOOT_STAGE_RW_SELECTKERNEL] =
		{"rw_selectkernel", vboot_rw_select_kernel,},
 	[VBOOT_STAGE_RW_BOOTKERNEL] = {"rw_bootkernel", vboot_rw_boot_kernel,},
#endif

#if 0
	/* For VB2, when supported */
	[VBOOT_STAGE_RW_KERNEL_PHASE1] =
		{"rw_kernel_phase1", vboot_rw_kernel_phase1,},
	[VBOOT_STAGE_RW_KERNEL_PHASE2] =
		{"rw_kernel_phase2", vboot_rw_kernel_phase2,},
	[VBOOT_STAGE_RW_KERNEL_PHASE3] =
		{"rw_kernel_phase3", vboot_rw_kernel_phase3,},
	[VBOOT_STAGE_RW_KERNEL_BOOT] = { "rw_kernel_boot", vboot_rw_kernel_boot, },
#endif
};

const char *vboot_get_stage_name(enum vboot_stage_t stagenum)
{
	if (stagenum >= VBOOT_STAGE_FIRST && stagenum < VBOOT_STAGE_COUNT)
		return stages[stagenum].name;

	return NULL;
}

enum vboot_stage_t vboot_find_stage(const char *name)
{
	enum vboot_stage_t stagenum;

	for (stagenum = VBOOT_STAGE_FIRST; stagenum < VBOOT_STAGE_COUNT;
	     stagenum++) {
		struct vboot_stage *stage = &stages[stagenum];

		if (!strcmp(name, stage->name))
			return stagenum;
	}

	return VBOOT_STAGE_NONE;
}

int vboot_run_stage(struct vboot_info *vboot, enum vboot_stage_t stagenum)
{
	struct vboot_stage *stage = &stages[stagenum];
	int ret;

	vboot_set_legacy(false);
	VB2_DEBUG("Running stage '%s'\n", stage->name);
	if (!stage->run) {
		VB2_DEBUG("   - Stage '%s' not available\n", stage->name);
		return -EPERM;
	}

	bootstage_mark_name(BOOTSTAGE_VBOOT_FIRST + stagenum, stage->name);
	ret = (*stage->run)(vboot);
	if (ret)
		VB2_DEBUG("Error: stage '%s' returned %d\n", stage->name, ret);

	return ret;
}

/**
 * Save non-volatile and/or secure data if needed.
 */
static int save_if_needed(struct vboot_info *vboot)
{
	struct vb2_context *ctx = vboot_get_ctx(vboot);
	int ret;

	if (ctx->flags & VB2_CONTEXT_NVDATA_CHANGED) {
		log(LOGC_VBOOT, LOGL_INFO, "Saving nvdata\n");
		ret = cros_nvdata_write_walk(CROS_NV_DATA, ctx->nvdata,
					     sizeof(ctx->nvdata));
		if (ret)
			return log_msg_ret("save nvdata", ret);
		ctx->flags &= ~VB2_CONTEXT_NVDATA_CHANGED;
	}
	if (ctx->flags & VB2_CONTEXT_SECDATA_CHANGED) {
		log(LOGC_VBOOT, LOGL_INFO, "Saving secdata\n");
		ret = cros_nvdata_write_walk(CROS_NV_SECDATA, ctx->nvdata,
					     sizeof(ctx->nvdata));
		if (ret)
			return log_msg_ret("save secdata", ret);
// 		antirollback_write_space_firmware(vboot);
		ctx->flags &= ~VB2_CONTEXT_SECDATA_CHANGED;
	}

	return 0;
}

int vboot_run_stages(struct vboot_info *vboot, enum vboot_stage_t start,
		     uint flags)
{
	enum vboot_stage_t stagenum;
	int ret;

	for (stagenum = start; stagenum < VBOOT_STAGE_COUNT; stagenum++) {
		if (!stages[stagenum].name)
			break;
		ret = vboot_run_stage(vboot, stagenum);
		save_if_needed(vboot);
		if (ret)
			break;
	}

#if CONFIG_IS_ENABLED(SYS_MALLOC_SIMPLE)
	malloc_simple_info();
#endif

	/* Allow dropping to the command line here for debugging */
	if (flags & VBOOT_FLAG_CMDLINE)
		return -EPERM;

	if (ret == VB2_ERROR_API_PHASE1_RECOVERY) {
		struct fmap_firmware_entry *fw = &vboot->fmap.readonly;
		struct vb2_context *ctx = vboot_get_ctx(vboot);

		vboot_set_selected_region(vboot, &fw->spl_rec, &fw->boot_rec);
		ret = vboot_fill_handoff(vboot);
		if (ret)
			return log_msg_ret("Cannot setup vboot handoff", ret);
		vboot_log(LOGL_WARNING, "flags %x %d\n", ctx->flags,
			  (ctx->flags & VB2_CONTEXT_RECOVERY_MODE) != 0);
		ctx->flags |= VB2_CONTEXT_RECOVERY_MODE;
// 		cold_reboot();
		return -ENOENT;  /* Try next boot method (which is recovery) */
	}

	if (ret == VBERROR_REBOOT_REQUIRED) {
		vboot_log(LOGL_WARNING, "Cold reboot\n");
		ret = cold_reboot();
	} else {
		switch (vboot->vb_error) {
		case VBERROR_BIOS_SHELL_REQUESTED:
			return -EPERM;
		case VBERROR_EC_REBOOT_TO_RO_REQUIRED:
		case VBERROR_SHUTDOWN_REQUESTED:
			vboot_log(LOGL_WARNING, "Power off\n");
			power_off();
			break;
		default:
// 			vboot_log(LOGL_WARNING, "Cold reboot\n");
// 			cold_reboot();
			break;
		}
	}

	return 0;
}

int vboot_run_auto(struct vboot_info *vboot, uint flags)
{
	enum vboot_stage_t stage;

	VB2_DEBUG("start\n");

	if (is_processor_reset())
		stage = VBOOT_STAGE_FIRST_VER;
	else
#ifdef CONFIG_SPL_BUILD
		stage = VBOOT_STAGE_FIRST_SPL;
#else
		stage = VBOOT_STAGE_FIRST_RW;
#endif

	return vboot_run_stages(vboot, stage, flags);
}

void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = BOOT_DEVICE_CROS_VBOOT;
	spl_boot_list[1] = BOOT_DEVICE_BOARD;
}

#ifdef CONFIG_TPL_BUILD
static int cros_load_image_tpl(struct spl_image_info *spl_image,
                               struct spl_boot_device *bootdev)
{
	struct vboot_info *vboot;
	int ret;

	printf("tpl: load image\n");
	ret = vboot_alloc(&vboot);
	if (ret)
		return ret;
	vboot->spl_image = spl_image;

	return vboot_run_auto(vboot, 0);
}
SPL_LOAD_IMAGE_METHOD("chromium_vboot_tpl", 0, BOOT_DEVICE_CROS_VBOOT,
		      cros_load_image_tpl);

#elif defined(CONFIG_SPL_BUILD)
static int cros_load_image_spl(struct spl_image_info *spl_image,
                               struct spl_boot_device *bootdev)
{
	struct vboot_info *vboot;
	int ret;

	printf("spl_load image\n");
	ret = vboot_alloc(&vboot);
	if (ret)
		return ret;
	vboot->spl_image = spl_image;

	return vboot_run_auto(vboot, 0);
}
SPL_LOAD_IMAGE_METHOD("chromium_vboot_spl", 0, BOOT_DEVICE_CROS_VBOOT,
		      cros_load_image_spl);
#endif /* CONFIG_SPL_BUILD */
