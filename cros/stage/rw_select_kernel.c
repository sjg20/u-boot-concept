/*
 * Copyright (c) 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <cros_ec.h>
#include <ec_commands.h>
#include <mapmem.h>
#include <cros/vboot.h>
#include <cros/power_management.h>

int vboot_rw_select_kernel(struct vboot_info *vboot)
{
	VbSelectAndLoadKernelParams *kparams = &vboot->kparams;
	fdt_addr_t kaddr;
	fdt_size_t ksize;
	ofnode config;
	int ret;

	config = cros_ofnode_config_node();
	if (!ofnode_valid(config))
		return log_msg_ret("Cannot find config node", -ENOENT);
	kaddr = ofnode_get_addr_size(config, "kernel-addr", &ksize);
	vboot->config = config;
	kaddr = 0x01008000;
	ksize = 0x2000000;
	if (kaddr == FDT_ADDR_T_NONE)
		return log_msg_ret("Cannot read kernel address", -EINVAL);
	kparams->kernel_buffer = map_sysmem(kaddr, ksize);
	kparams->kernel_buffer_size = ksize;

	if (IS_ENABLED(CONFIG_DETACHABLE_UI) &&
	    vboot_config_bool(vboot, "detachable-ui")) {
		kparams->inflags = VB_SALK_INFLAGS_ENABLE_DETACHABLE_UI;
		if (IS_ENABLED(CONFIG_X86) &&
		    CONFIG_IS_ENABLED(CROS_EC)) {
			// On x86 systems, inhibit power button pulse from EC.
// 			cros_ec_config_powerbtn(0);
// 			list_insert_after(&x86_ec_powerbtn_cleanup.list_node,
// 					  &cleanup_funcs);
		}
	}

	printf("Calling VbSelectAndLoadKernel().\n");
	VbError_t res = VbSelectAndLoadKernel(&vboot->cparams, kparams);

	ret = 0;
	if (res == VBERROR_EC_REBOOT_TO_RO_REQUIRED) {
		printf("EC Reboot requested. Doing cold reboot.\n");
		if (CONFIG_IS_ENABLED(CROS_EC))
			cros_ec_reboot(vboot->cros_ec, EC_REBOOT_COLD, 0);
		ret = power_off();
	} else if (res == VBERROR_EC_REBOOT_TO_SWITCH_RW) {
		printf("Switch EC slot requested. Doing cold reboot.\n");
		if (CONFIG_IS_ENABLED(CROS_EC))
			cros_ec_reboot(vboot->cros_ec, EC_REBOOT_COLD,
				       EC_REBOOT_FLAG_SWITCH_RW_SLOT);
		ret = power_off();
	} else if (res == VBERROR_SHUTDOWN_REQUESTED) {
		printf("Powering off.\n");
		ret = power_off();
	} else if (res == VBERROR_REBOOT_REQUIRED) {
		printf("Reboot requested. Doing warm reboot.\n");
		ret = cold_reboot();
	}
	if (res != VBERROR_SUCCESS) {
		printf("VbSelectAndLoadKernel returned %d, "
		       "Doing a cold reboot.\n", res);
		ret = cold_reboot();
	}
	if (ret)
		return log_msg_ret("Failed to reboot/power off", ret);

	return 0;
}
