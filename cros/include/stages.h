/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

struct vboot_info;

enum vboot_stage_t {
	VBOOT_STAGE_FIRST = 0,
	VBOOT_STAGE_FIRST_VER = VBOOT_STAGE_FIRST,
	VBOOT_STAGE_VER_INIT = VBOOT_STAGE_FIRST,
	VBOOT_STAGE_VER1_VBINIT,
	VBOOT_STAGE_VER2_SELECTFW,
	VBOOT_STAGE_VER3_TRYFW,
	VBOOT_STAGE_VER4_LOCATEFW,
	VBOOT_STAGE_VER_FINISH,
	VBOOT_STAGE_VER_JUMP,

	VBOOT_STAGE_FIRST_SPL,
	VBOOT_STAGE_SPL_INIT = VBOOT_STAGE_FIRST_SPL,
	VBOOT_STAGE_SPL_JUMP_U_BOOT,

	VBOOT_STAGE_FIRST_RW,
	VBOOT_STAGE_RW_INIT = VBOOT_STAGE_FIRST_RW,
	VBOOT_STAGE_RW_SELECTKERNEL,
	VBOOT_STAGE_RW_BOOTKERNEL,

	/* VB2 stages, not yet implemented */
	VBOOT_STAGE_RW_KERNEL_PHASE1,
	VBOOT_STAGE_RW_KERNEL_PHASE2,
	VBOOT_STAGE_RW_KERNEL_PHASE3,
	VBOOT_STAGE_RW_KERNEL_BOOT,

	VBOOT_STAGE_COUNT,
	VBOOT_STAGE_NONE,
};

/* Flags to use for running stages */
enum vboot_stage_flag_t {
	VBOOT_FLAG_CMDLINE	= 1 << 0,	/* drop to cmdline on error */
};

const char *vboot_get_stage_name(enum vboot_stage_t stagenum);
enum vboot_stage_t vboot_find_stage(const char *name);
int vboot_run_stage(struct vboot_info *vboot, enum vboot_stage_t stage);
int vboot_run_stages(struct vboot_info *vboot, enum vboot_stage_t start,
		     uint flags);

/*
 * vboot_run_auto() - Run verified boot automatically
 *
 * This selects the correct stage to start from, and runs through all the
 * stages from then on. The result will normally be jumping to the next stage
 * of U-Boot or the kernel.
 *
 * @return does not return in the normal case, returns -ve value on error
 */
int vboot_run_auto(struct vboot_info *vboot, uint flags);

int vboot_ver_init(struct vboot_info *vboot);
int vboot_ver1_vbinit(struct vboot_info *vboot);
int vboot_ver2_select_fw(struct vboot_info *vboot);
int vboot_ver3_try_fw(struct vboot_info *vboot);
int vboot_ver4_locate_fw(struct vboot_info *vboot);
int vboot_ver5_finish_fw(struct vboot_info *vboot);
int vboot_ver6_jump_fw(struct vboot_info *vboot);

int vboot_spl_init(struct vboot_info *vboot);
int vboot_spl_jump_u_boot(struct vboot_info *vboot);

int vboot_rw_init(struct vboot_info *vboot);
int vboot_rw_select_kernel(struct vboot_info *vboot);
int vboot_rw_boot_kernel(struct vboot_info *vboot);

int vboot_rw_kernel_phase1(struct vboot_info *vboot);
int vboot_rw_kernel_phase2(struct vboot_info *vboot);
int vboot_rw_kernel_phase3(struct vboot_info *vboot);
int vboot_rw_kernel_boot(struct vboot_info *vboot);
