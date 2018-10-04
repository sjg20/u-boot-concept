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
#include <cros_ec.h>
#include <malloc.h>
#include <vb2_api.h>
#include <cros/cros_common.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>
#include <cros/vboot_ec.h>
#include <cros/aux_fw.h>

DECLARE_GLOBAL_DATA_PTR;

/* We support corrupting a byte of the EC image */
static int corrupt_offset = -1;	/* offset into image to corrupt */
static uint8_t corrupt_byte;		/* new byte value at that offset */


void cros_ec_set_corrupt_image(int offset, int byte)
{
	corrupt_offset = offset;
	corrupt_byte = byte;
}

int VbExTrustEC(int devidx)
{
	int gpio_ec_in_rw;
	int okay;

	printf("%s: %d\n", __func__, devidx);
	if (devidx != 0)
		return 0;

	/* If we don't have a valid GPIO to read, we can't trust it. */
	gpio_ec_in_rw = vboot_flag_read_walk(VBOOT_FLAG_EC_IN_RW);
	if (gpio_ec_in_rw < 0) {
		VB2_DEBUG("can't find GPIO to read, returning 0\n");
		return 0;
	}

	/* We only trust it if it's NOT in its RW firmware. */
	okay = !gpio_ec_in_rw;

	VB2_DEBUG("value=%d, returning %d\n",gpio_ec_in_rw, okay);

	return okay;
}

static int ec_get(int devidx, struct udevice **devp)
{
	struct vboot_info *vboot = vboot_get();
	struct udevice *dev;
	int ret;

	if (devidx == 0) {
		*devp = vboot->cros_ec;
		return 0;
	}
	ret = uclass_get_device_by_seq(UCLASS_CROS_VBOOT_EC, devidx, &dev);
	if (ret) {
		vboot_log(LOGL_ERR, "Get EC: err=%d", ret);
		return VBERROR_UNKNOWN;
	}
	*devp = dev;

	return 0;
}

VbError_t VbExEcRunningRW(int devidx, int *in_rw)
{
	struct udevice *dev;
	int ret;

	printf("%s: %d\n", __func__, devidx);
	ret = ec_get(devidx, &dev);
	if (ret)
		return log_msg_ret("Cannot get EC", ret);

	ret = vboot_ec_running_rw(dev, in_rw);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcJumpToRW(int devidx)
{
	struct udevice *dev;
	int ret;

	printf("%s: %d\n", __func__, devidx);
	ret = ec_get(devidx, &dev);
	if (ret)
		return log_msg_ret("Cannot get EC", ret);

	ret = vboot_ec_jump_to_rw(dev);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcDisableJump(int devidx)
{
	struct udevice *dev;
	int ret;

	printf("%s: %d\n", __func__, devidx);
	ret = uclass_get_device_by_seq(UCLASS_CROS_VBOOT_EC, devidx, &dev);
	ret = ec_get(devidx, &dev);
	if (ret)
		return log_msg_ret("Cannot get EC", ret);

	ret = vboot_ec_disable_jump(dev);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

#if 0
VbError_t VbExEcHashRW(int devidx, const uint8_t **hash, int *hash_size)
{
	struct udevice *dev = board_get_cros_ec_dev();
	static struct ec_response_vboot_hash resp __aligned(8);

	if (devidx != 0)
		return VBERROR_UNKNOWN;

	if (!dev) {
		VB2_DEBUG("%s: no cros_ec device\n", __func__);
		return VBERROR_UNKNOWN;
	}

	if (cros_ec_read_hash(dev, EC_VBOOT_HASH_OFFSET_ACTIVE, &resp) < 0)
		return VBERROR_UNKNOWN;

	if (resp.status != EC_VBOOT_HASH_STATUS_DONE)
		return VBERROR_UNKNOWN;
	if (resp.hash_type != EC_VBOOT_HASH_TYPE_SHA256)
		return VBERROR_UNKNOWN;

	*hash = resp.hash_digest;
	*hash_size = resp.digest_size;

	return VBERROR_SUCCESS;
}

static VbError_t ec_protect_rw(int protect)
{
	struct udevice *dev = board_get_cros_ec_dev();
	struct ec_response_flash_protect resp;
	uint32_t mask = EC_FLASH_PROTECT_ALL_NOW | EC_FLASH_PROTECT_ALL_AT_BOOT;

	if (!dev) {
		VB2_DEBUG("%s: no cros_ec device\n", __func__);
		return VBERROR_UNKNOWN;
	}

	/* Update protection */
	if (cros_ec_flash_protect(dev, mask, protect ? mask : 0, &resp) < 0)
		return VBERROR_UNKNOWN;

	if (!protect) {
		/* If protection is still enabled, need reboot */
		if (resp.flags & EC_FLASH_PROTECT_ALL_NOW)
			return VBERROR_EC_REBOOT_TO_RO_REQUIRED;

		return VBERROR_SUCCESS;
	}

	/*
	 * If write protect and ro-at-boot aren't both asserted, don't expect
	 * protection enabled.
	 */
	if ((~resp.flags) & (EC_FLASH_PROTECT_GPIO_ASSERTED |
			     EC_FLASH_PROTECT_RO_AT_BOOT))
		return VBERROR_SUCCESS;

	/* If flash is protected now, success */
	if (resp.flags & EC_FLASH_PROTECT_ALL_NOW)
		return VBERROR_SUCCESS;

	/* If RW will be protected at boot but not now, need a reboot */
	if (resp.flags & EC_FLASH_PROTECT_ALL_AT_BOOT)
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;

	/* Otherwise, it's an error */
	return VBERROR_UNKNOWN;
}

VbError_t VbExEcUpdateRW(int devidx, const uint8_t  *image, int image_size)
{
	struct udevice *dev = board_get_cros_ec_dev();
	int rv;

	if (devidx != 0)
		return VBERROR_UNKNOWN;

	if (!dev) {
		VB2_DEBUG("%s: no cros_ec device\n", __func__);
		return VBERROR_UNKNOWN;
	}

	rv = ec_protect_rw(0);
	if (rv == VBERROR_EC_REBOOT_TO_RO_REQUIRED)
		return rv;
	else if (rv != VBERROR_SUCCESS)
		return rv;

	rv = cros_ec_flash_update_rw(dev, image, image_size);
	return rv == 0 ? VBERROR_SUCCESS : VBERROR_UNKNOWN;
}

VbError_t VbExEcProtectRW(int devidx)
{
	if (devidx != 0)
		return VBERROR_UNKNOWN;

	return ec_protect_rw(1);
}
#endif

VbError_t VbExEcHashImage(int devidx, enum VbSelectFirmware_t select,
			  const uint8_t **hashp, int *hash_sizep)
{
	struct udevice *dev;
	int ret;

	ret = ec_get(devidx, &dev);
	if (ret)
		return log_msg_ret("Cannot get EC", ret);

	ret = vboot_ec_hash_image(dev, select, hashp, hash_sizep);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

static struct fmap_entry *get_firmware_entry(struct vboot_info *vboot,
					     int devidx,
					     enum VbSelectFirmware_t select)
{
	struct fmap_firmware_entry *fw;
	struct fmap_firmware_ec *ec;
	struct fmap_entry *entry;

	fw = vboot_is_slot_a(vboot) ? &vboot->fmap.readwrite_a :
		&vboot->fmap.readwrite_b;
	if (devidx < 0 || devidx >= EC_COUNT) {
		vboot_log(LOGL_ERR,
			  "entry not found, slot=%s, devidx=%d, select=%d",
			  vboot_slot_name(vboot), devidx, select);
		return NULL;
	}
	ec = &fw->ec[devidx];
	entry = select == VB_SELECT_FIRMWARE_READONLY ? &ec->ro : &ec->rw;
	vboot_log(LOGL_DEBUG, "Selected devidx=%d, select=%s\n", devidx,
		  select == VB_SELECT_FIRMWARE_READONLY ? "ro" : "rw");

	return entry;
}

VbError_t VbExEcGetExpectedImage(int devidx, enum VbSelectFirmware_t select,
				 const uint8_t **imagep, int *image_sizep)
{
	struct vboot_info *vboot = vboot_get();
	struct fmap_entry *entry;
	u8 *image;
	int ret;

	printf("%s: %d\n", __func__, devidx);
	entry = get_firmware_entry(vboot, devidx, select);
	if (!entry)
		return VBERROR_UNKNOWN;

	ret = fwstore_load_image(vboot->fwstore, entry, &image, image_sizep);
	if (ret) {
		vboot_log(LOGL_ERR, "Cannot locate image: err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}
	*imagep = image;

	return VBERROR_SUCCESS;
}

VbError_t VbExEcGetExpectedImageHash(int devidx, enum VbSelectFirmware_t select,
				     const uint8_t **hash, int *hash_size)
{
	struct vboot_info *vboot = vboot_get();
	struct fmap_entry *entry;

	printf("%s: %d\n", __func__, devidx);
	entry = get_firmware_entry(vboot, devidx, select);
	if (!entry)
		return VBERROR_UNKNOWN;
	*hash = entry->hash;
	*hash_size = entry->hash_size;

	return VBERROR_SUCCESS;
}

VbError_t VbExEcUpdateImage(int devidx, enum VbSelectFirmware_t select,
			    const uint8_t *image, int image_size)
{
	struct udevice *dev;
	int ret;

	printf("%s: %d\n", __func__, devidx);
	ret = ec_get(devidx, &dev);
	if (ret)
		return log_msg_ret("Cannot get EC", ret);

	ret = vboot_ec_update_image(dev, select, image, image_size);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		switch (ret) {
		case -EINVAL:
			return VBERROR_INVALID_PARAMETER;
		case -EPERM:
			return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
		case -EIO:
		default:
			return VBERROR_UNKNOWN;
		}
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcProtect(int devidx, enum VbSelectFirmware_t select)
{
	struct udevice *dev;
	int ret;

	printf("%s: %d\n", __func__, devidx);
	ret = ec_get(devidx, &dev);
	if (ret)
		return log_msg_ret("Cannot get EC", ret);

	ret = vboot_ec_protect(dev, select);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExEcEnteringMode(int devidx, enum VbEcBootMode_t mode)
{
	struct udevice *dev;
	int ret;

	printf("%s: %d\n", __func__, devidx);
	ret = ec_get(devidx, &dev);
	if (ret)
		return log_msg_ret("Cannot get EC", ret);

	ret = vboot_ec_entering_mode(dev, mode);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

#if 0
VbError_t VbExEcGetExpectedRW(int devidx, enum VbSelectFirmware_t select,
			      const uint8_t **image, int *image_size)
{
	struct fmap_firmware_entry *fw_entry;
	struct twostop_fmap fmap;
	uint32_t offset;
	fwstore_t file;
	uint8_t *buf;
	int size;

	*image = NULL;
	*image_size = 0;

	if (devidx != 0)
		return VBERROR_UNKNOWN;

	cros_ofnode_flashmap(&fmap);
	switch (select) {
	case VB_SELECT_FIRMWARE_A:
		fw_entry = &fmap.readwrite_a;
		break;
	case VB_SELECT_FIRMWARE_B:
		fw_entry = &fmap.readwrite_b;
		break;
	default:
		VB2_DEBUG("Unrecognized EC firmware requested\n");
		return VBERROR_UNKNOWN;
	}

	if (fwstore_open(&file)) {
		VB2_DEBUG("Failed to open firmware storage\n");
		return VBERROR_UNKNOWN;
	}

	offset = fw_entry->ec_rw.offset;
	size = fw_entry->ec_rw.used;
	VB2_DEBUG("EC-RW image offset %#x used %#x\n", offset, size);

	/* Sanity-check; we don't expect EC images > 1MB */
	if (size <= 0 || size > 0x100000) {
		VB2_DEBUG("EC image has bogus size\n");
		return VBERROR_UNKNOWN;
	}

	/*
	 * Note: this leaks memory each call, since we don't track the pointer
	 * and the caller isn't expecting to free it.
	 */
	buf = malloc(size);
	if (!buf) {
		VB2_DEBUG("Failed to allocate space for the EC RW image\n");
		return VBERROR_UNKNOWN;
	}

	if (file.read(&file, offset, size, buf)) {
		free(buf);
		VB2_DEBUG("Failed to read the EC from flash\n");
		return VBERROR_UNKNOWN;
	}

	file.close(&file);

	/* Corrupt a byte if requested */
	if (corrupt_offset >= 0 && corrupt_offset < size)
		buf[corrupt_offset] = corrupt_byte;

	*image = buf;
	*image_size = size;
	return VBERROR_SUCCESS;
}

VbError_t VbExEcGetExpectedRWHash(int devidx, enum VbSelectFirmware_t select,
		       const uint8_t **hash, int *hash_size)
{
	struct twostop_fmap fmap;
	struct fmap_entry *ec;

	*hash = NULL;

	if (devidx != 0)
		return VBERROR_UNKNOWN;

	/* TODO: Decode the flashmap once and reuse throughout. */
	cros_ofnode_flashmap(&fmap);
	switch (select) {
	case VB_SELECT_FIRMWARE_A:
		ec = &fmap.readwrite_a.ec_rw;
		break;
	case VB_SELECT_FIRMWARE_B:
		ec = &fmap.readwrite_b.ec_rw;
		break;
	default:
		return VBERROR_UNKNOWN;
	}
	if (!ec->hash) {
		VB2_DEBUG("Failed to read EC hash\n");
		return VBERROR_UNKNOWN;
	}
	*hash = ec->hash;
	*hash_size = ec->hash_size;

	return VBERROR_SUCCESS;
}
#endif

/* Wait 3 seconds after software sync for EC to clear the limit power flag. */
#define LIMIT_POWER_WAIT_TIMEOUT 3000
/* Check the limit power flag every 50 ms while waiting. */
#define LIMIT_POWER_POLL_SLEEP 50

VbError_t VbExEcVbootDone(int in_recovery)
{
	struct udevice *dev = board_get_cros_ec_dev();
	int limit_power;

	printf("%s\n", __func__);
	/* Ensure we have enough power to continue booting */
	while (1) {
		bool message_printed = false;
		int limit_power_wait_time = 0;
		int ret;

		ret = cros_ec_read_limit_power(dev, &limit_power);
		if (ret == -ENOSYS) {
			limit_power = 0;
		} else if (ret) {
			vboot_log(LOGL_WARNING,
				  "Failed to check EC limit power flag\n");
			return VBERROR_UNKNOWN;
		}

		/*
		 * Do not wait for the limit power flag to be cleared in
		 * recovery mode since we didn't just sysjump.
		 */
		if (!limit_power || in_recovery ||
		    limit_power_wait_time > LIMIT_POWER_WAIT_TIMEOUT)
			break;

		if (!message_printed) {
			vboot_log(LOGL_INFO,
				  "Waiting for EC to clear limit power flag\n");
			message_printed = 1;
		}

		mdelay(LIMIT_POWER_POLL_SLEEP);
		limit_power_wait_time += LIMIT_POWER_POLL_SLEEP;
	}

	if (limit_power) {
		vboot_log(LOGL_INFO,
			  "EC requests limited power usage. Request shutdown\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}

	bootstage_mark(BOOTSTAMP_VBOOT_EC_DONE);

	return VBERROR_SUCCESS;
}

VbError_t VbExEcBatteryCutOff(void)
{
	struct udevice *dev = board_get_cros_ec_dev();
	int ret;

	printf("%s\n", __func__);
	ret = cros_ec_battery_cutoff(dev, EC_BATTERY_CUTOFF_FLAG_AT_SHUTDOWN);
	if (ret) {
		vboot_log(LOGL_ERR, "Failed, err=%d\n", ret);
		return VBERROR_UNKNOWN;
	}

	return VBERROR_SUCCESS;
}

static int locate_aux_fw(struct udevice *dev, struct fmap_entry *entry)
{
	struct ofnode_phandle_args args;
	int ret;

	ret = ofnode_parse_phandle_with_args(dev_ofnode(dev), "firmware", NULL,
					     0, 0, &args);
	if (ret)
		return log_msg_ret("Cannot find firmware", ret);
	ret = ofnode_read_fmap_entry(args.node, entry);
	if (ret)
		return log_msg_ret("Cannot read fmap entry", ret);

	return 0;
}

VbError_t VbExCheckAuxFw(VbAuxFwUpdateSeverity_t *severityp)
{
	enum aux_fw_severity max, current;
	struct udevice *dev;
	struct fmap_entry entry;
	int ret;

	max = VB_AUX_FW_NO_UPDATE;
	for (uclass_first_device(UCLASS_CROS_AUX_FW, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		ret = locate_aux_fw(dev, &entry);
		if (ret)
			return ret;
		if (!entry.hash)
			return log_msg_ret("Entry has no hash", -EINVAL);
		ret = aux_fw_check_hash(dev, entry.hash, entry.hash_size,
					&current);
		if (ret)
			return log_msg_ret("Check hashf failed", ret);
		max = max(max, current);
	}
	switch (max) {
	case AUX_FW_NO_UPDATE:
		*severityp = VB_AUX_FW_NO_UPDATE;
		break;
	case AUX_FW_FAST_UPDATE:
		*severityp = AUX_FW_FAST_UPDATE;
		break;
	case AUX_FW_SLOW_UPDATE:
		*severityp = AUX_FW_SLOW_UPDATE;
		break;
	default:
		vboot_log(LOGL_ERR, "Invalid severity %d", max);
		return VBERROR_UNKNOWN;
	}

	return 0;
}

struct aux_fw_state {
	bool power_button_disabled;
	bool lid_shutdown_disabled;
	bool reboot_required;
};

static int do_aux_fw_update(struct vboot_info *vboot, struct udevice *dev,
			    struct aux_fw_state *state)
{
	enum aux_fw_severity severity;
	struct fmap_entry entry;
	u8 *image;
	int size;
	int ret;

	if (!state->power_button_disabled &&
	    vboot_config_bool(vboot, "disable-power-button-during-update")) {
		cros_ec_config_powerbtn(vboot->cros_ec, 0);
		state->power_button_disabled = 1;
	}
	if (!state->lid_shutdown_disabled &&
	    vboot_config_bool(vboot, "disable-lid-shutdown-if-enabled") &&
		cros_ec_get_lid_shutdown_mask(vboot->cros_ec) > 0) {
		if (!cros_ec_set_lid_shutdown_mask(vboot->cros_ec, 0))
			state->lid_shutdown_disabled = 1;
	}
	/* Apply update */
	ret = locate_aux_fw(dev, &entry);
	if (ret)
		return ret;

	vboot_log(LOGL_INFO, "Update aux fw '%s'\n", dev->name);
	ret = fwstore_load_image(dev, &entry, &image, &size);
	
	ret = aux_fw_update_image(dev, image, size);
	if (ret == ERESTARTSYS)
		state->reboot_required = 1;
	else if (ret)
		return ret;
	/* Re-check hash after update */
	ret = aux_fw_check_hash(dev, entry.hash, entry.hash_size, &severity);
	if (ret)
		return log_msg_ret("Check hash failed", ret);
	if (severity != AUX_FW_NO_UPDATE)
		return -EIO;

	return 0;
}

VbError_t VbExUpdateAuxFw(void)
{
	struct vboot_info *vboot = vboot_get();
	struct aux_fw_state state = {0};
	struct udevice *dev;
	int ret;

	printf("%s\n", __func__);
	for (uclass_first_device(UCLASS_CROS_AUX_FW, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		enum aux_fw_severity severity = aux_fw_get_severity(dev);

		if (severity != AUX_FW_NO_UPDATE) {
			ret = do_aux_fw_update(vboot, dev, &state);
			if (ret) {
				vboot_log(LOGL_ERR,
					  "Update for '%s' failed: err=%d\n",
					  dev->name, ret);
				break;
			}
		}
		vboot_log(LOGL_INFO, "Protect aux fw '%s'\n", dev->name);
		ret = aux_fw_protect(dev);
		if (ret) {
			vboot_log(LOGL_ERR, "Update for '%s' failed: err=%d\n",
				  dev->name, ret);
			break;
		}
	}
	/* Re-enable power button after update, if required */
	if (state.power_button_disabled)
		cros_ec_config_powerbtn(vboot->cros_ec,
					EC_POWER_BUTTON_ENABLE_PULSE);

	/* Re-enable lid shutdown event, if required */
	if (state.lid_shutdown_disabled)
		cros_ec_set_lid_shutdown_mask(vboot->cros_ec, 1);

	/* Request EC reboot, if required */
	if (state.reboot_required && !ret)
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;

	return 0;
}
