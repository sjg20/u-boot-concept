// SPDX-License-Identifier: GPL-2.0+
/*
 * Sets up the read-write vboot portion (which loads the kernel)
 *
 * Copyright 2018 Google LLC
 */

#define NEED_VB20_INTERNALS

#include <common.h>
#include <bloblist.h>
#include <cb_sysinfo.h>
#include <dm.h>
#include <init.h>
#include <log.h>
#include <mapmem.h>
#include <cros/cros_ofnode.h>
#include <cros/fmap.h>
#include <cros/fwstore.h>
#include <cros/keyboard.h>
#include <cros/nvdata.h>
#include <cros/vboot.h>
#include <cros/vboot_flag.h>
#include <cros/memwipe.h>

#include <gbb_header.h>
#include <vboot_struct.h>

#include <tpm-common.h>

/**
 * gbb_copy_in() - Copy a portion of the GBB into vboot->cparams
 *
 * @vboot:	Vboot info
 * @gbb_offset:	Offset of GBB in vboot->fwstore
 * @offset:	Offset within GBB to read
 * @size:	Number of bytes to read
 * @return 0 if OK, -EINVAL if offset/size invalid, other error if fwstore
 *	fails to read
 */
static int gbb_copy_in(struct vboot_info *vboot, uint gbb_offset, uint offset,
		       uint size)
{
	VbCommonParams *cparams = &vboot->cparams;
	u8 *gbb_copy = cparams->gbb_data;
	int ret;

	if (offset > cparams->gbb_size || offset + size > cparams->gbb_size)
		return log_msg_ret("range", -EINVAL);
	ret = cros_fwstore_read(vboot->fwstore, gbb_offset + offset, size,
				gbb_copy + offset);
	if (ret)
		return log_msg_ret("read", ret);

	return 0;
}

static int gbb_init(struct vboot_info *vboot)
{
	struct fmap_entry *entry = &vboot->fmap.readonly.gbb;
	VbCommonParams *cparams = &vboot->cparams;
	u32 offset;
	int ret, i;

	cparams->gbb_size = entry->length;
	cparams->gbb_data = malloc(entry->length);
	if (!cparams->gbb_data)
		return log_msg_ret("buffer", -ENOMEM);

	memset(cparams->gbb_data, 0, cparams->gbb_size);

	offset = entry->offset;

	GoogleBinaryBlockHeader *hdr = cparams->gbb_data;

	ret = gbb_copy_in(vboot, offset, 0, sizeof(GoogleBinaryBlockHeader));
	if (ret)
		return ret;
	log_debug("The GBB signature is at %p and is:", hdr->signature);
	for (i = 0; i < GBB_SIGNATURE_SIZE; i++)
		log_debug(" %02x", hdr->signature[i]);
	log_debug("\n");

	ret = gbb_copy_in(vboot, offset, hdr->hwid_offset, hdr->hwid_size);
	if (ret)
		return ret;

	ret = gbb_copy_in(vboot, offset, hdr->rootkey_offset,
			  hdr->rootkey_size);
	if (ret)
		return ret;

	ret = gbb_copy_in(vboot, offset, hdr->recovery_key_offset,
			  hdr->recovery_key_size);
	if (ret)
		return ret;

	return 0;
}

static int common_params_init(struct vboot_info *vboot, bool clear_shared_data)
{
	VbCommonParams *cparams = &vboot->cparams;
	int ret;

	memset(cparams, '\0', sizeof(*cparams));

	ret = gbb_init(vboot);
	if (ret)
		return log_msg_ret("gbb", ret);

	cparams->shared_data_blob = vboot->handoff->shared_data;
	cparams->shared_data_size = ARRAY_SIZE(vboot->handoff->shared_data);
	if (clear_shared_data)
		memset(cparams->shared_data_blob, '\0',
		       cparams->shared_data_size);
	log_info("Found shared_data_blob at %lx, size %d\n",
		 (ulong)map_to_sysmem(cparams->shared_data_blob),
		 cparams->shared_data_size);

	return 0;
}

/* When running under coreboot, use the coreboot tables to find memory */
#if defined(CONFIG_SYS_COREBOOT)
static void setup_arch_unused_memory(struct vboot_info *vboot,
				     struct memwipe *wipe)
{
	int i;

	/* Add ranges that describe RAM */
	for (i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];

		if (range->type == CB_MEM_RAM) {
			memwipe_add(wipe, range->base,
				    range->base + range->size);
		}
	}
	/*
	 * Remove ranges that don't. These should take precedence, so they're
	 * done last and in their own loop.
	 */
	for (i = 0; i < lib_sysinfo.n_memranges; i++) {
		struct memrange *range = &lib_sysinfo.memrange[i];

		if (range->type != CB_MEM_RAM) {
			memwipe_sub(wipe, range->base,
				    range->base + range->size);
		}
	}
}

#else
static void setup_arch_unused_memory(struct vboot_info *vboot,
				     struct memwipe *wipe)
{
	struct fdt_memory ramoops, lp0;
	int bank;

	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		if (!gd->bd->bi_dram[bank].size)
			continue;
		memwipe_add(wipe, gd->bd->bi_dram[bank].start,
			    gd->bd->bi_dram[bank].start +
			    gd->bd->bi_dram[bank].size);
	}

	/* Excludes kcrashmem if in FDT */
	if (cros_ofnode_memory("/ramoops", &ramoops))
		log_debug("RAMOOPS not contained within FDT\n");
	else
		memwipe_sub(wipe, ramoops.start, ramoops.end);

	/* Excludes the LP0 vector; only applicable to tegra platforms */
	if (cros_ofnode_memory("/lp0", &lp0))
		log_debug("LP0 not contained within FDT\n");
	else
		memwipe_sub(wipe, lp0.start, lp0.end);
}
#endif

/**
 * get_current_sp() - Get an approximation of the stack pointer
 *
 * @return current stack-pointer value
 */
static ulong get_current_sp(void)
{
#ifdef CONFIG_SANDBOX
	return gd->start_addr_sp;
#else
	ulong addr, sp;

	sp = (ulong)&addr;

	return sp;
#endif
}

static void wipe_unused_memory(struct vboot_info *vboot)
{
	struct memwipe wipe;

	memwipe_init(&wipe);
	setup_arch_unused_memory(vboot, &wipe);

	/* Exclude relocated U-Boot structures */
	memwipe_sub(&wipe, get_current_sp() - MEMWIPE_STACK_MARGIN,
		    gd->ram_top);

	/* Exclude the shared data between bootstub and main firmware */
	memwipe_sub(&wipe, (ulong)vboot->handoff,
		    (ulong)vboot->handoff + sizeof(struct vboot_handoff));

	memwipe_execute(&wipe);
}

static int vboot_do_init_out_flags(struct vboot_info *vboot, u32 out_flags)
{
	if (0 && (out_flags & VB_INIT_OUT_CLEAR_RAM)) {
		if (vboot->disable_memwipe)
			log_warning("Memory wipe requested but not supported\n");
		else
			wipe_unused_memory(vboot);
	}

	vboot->vboot_out_flags = out_flags;

	return 0;
}

static int vboot_init_handoff(struct vboot_info *vboot)
{
	struct vboot_handoff *handoff;
	VbSharedDataHeader *vdat;
	int ret;

	if (!vboot->from_coreboot) {
		handoff = bloblist_find(BLOBLISTT_VBOOT_HANDOFF,
					sizeof(*handoff));
		if (!handoff)
			return log_msg_ret("handoff\n", -ENOENT);
		vboot->handoff = handoff;
	} else {
		handoff = vboot->sysinfo->vboot_handoff;
	}

	/* Set up the common param structure, not clearing shared data */
	ret = common_params_init(vboot, 0);
	if (ret)
		return ret;

	vdat = vboot->cparams.shared_data_blob;
	/*
	 * If the lid is closed, don't count down the boot tries for updates,
	 * since the OS will shut down before it can register success.
	 *
	 * VbInit() was already called in stage A, so we need to update the
	 * vboot internal flags ourself.
	 */
	if (vboot_flag_read_walk(VBOOT_FLAG_LID_OPEN) == 0) {
		/* Tell kernel selection to not count down */
		vdat->flags |= VBSD_NOFAIL_BOOT;
	}

	ret = vboot_do_init_out_flags(vboot, handoff->init_params.out_flags);
	if (ret)
		return log_msg_ret("flags", ret);

	return 0;
}

static int fmap_read(struct vboot_info *vboot)
{
	const struct sysinfo_t *sysinfo = vboot->sysinfo;
	struct fmap_entry entry;
	ulong addr;
	int ret;

	entry.offset = sysinfo->fmap_offset;
	entry.length = 0x1000;
	ret = fwstore_entry_mmap(vboot->fwstore, &entry, &addr);
	if (ret)
		return log_msg_ret("entry", ret);
	ret = fmap_parse((void *)addr, &vboot->fmap);
	if (ret)
		return log_msg_ret("parse", ret);

	return 0;
}

#if 0
int vboot_make_context(const struct sysinfo_t *sysinfo,
		       struct vb2_context **ctxp)
{
	const struct vboot_handoff *handoff = sysinfo->vboot_handoff;
	struct vb2_context *ctx;

	if (!handoff)
		return log_msg_ret("handoff", -ENOENT);
	log_info("Using vboot_handoff at %p\n", handoff);
	ctx = calloc(sizeof(*ctx), 1);
	if (!ctx)
		return log_msg_ret("ctx", -ENOMEM);
	ctx->workbuf_size = VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE;
	if (sysinfo->vboot_workbuf_size > ctx->workbuf_size)
		return log_msg_ret("workbuf", -ENOSPC);

	/*
	 * Import the firmware verification workbuf, which includes
	 * vb2_shared_data
	 * */
	ctx->workbuf = memalign(VB2_WORKBUF_ALIGN, ctx->workbuf_size);
	memcpy(ctx.workbuf, sysinfo->vboot_workbuf,
	       sysinfo->vboot_workbuf_size);
	ctx->workbuf_used = sysinfo->vboot_workbuf_size;

	*ctxp = ctx;

	return 0;
}
#endif

#if 0
static int vboot_init_handoff()
{
	struct vboot_handoff *vboot_handoff;

	// Set up the common param structure, not clearing shared data.
	if (common_params_init(0))
		return 1;

	if (lib_sysinfo.vboot_handoff == NULL) {
		printf("vboot handoff pointer is NULL\n");
		return 1;
	}

	if (lib_sysinfo.vboot_handoff_size != sizeof(struct vboot_handoff)) {
		printf("Unexpected vboot handoff size: %d\n",
		       lib_sysinfo.vboot_handoff_size);
		return 1;
	}

	vboot_handoff = lib_sysinfo.vboot_handoff;

	/* If the lid is closed, don't count down the boot
	 * tries for updates, since the OS will shut down
	 * before it can register success.
	 *
	 * VbInit was already called in coreboot, so we need
	 * to update the vboot internal flags ourself.
	 */
	int lid_switch = flag_fetch(FLAG_LIDSW);
	if (!lid_switch) {
		VbSharedDataHeader *vdat;
		int vdat_size;

		if (find_common_params((void **)&vdat, &vdat_size) != 0)
			vdat = NULL;

		/* We need something to work with */
		if (vdat != NULL)
			/* Tell kernel selection to not count down */
			vdat->flags |= VBSD_NOFAIL_BOOT;
	}

	return vboot_do_init_out_flags(vboot_handoff->init_params.out_flags);
}
#endif

int vboot_rw_init(struct vboot_info *vboot)
{
	struct fmap_section *fw_entry;
	struct vboot_blob *blob;
	struct vb2_context *ctx;
	int ret;

	if (ll_boot_init()) {
		blob = bloblist_find(BLOBLISTT_VBOOT_CTX, sizeof(*blob));
		if (!blob)
			return log_msg_ret("blob", -ENOENT);
		vboot->blob = blob;
		ctx = &blob->ctx;
		vboot->ctx = ctx;
		ctx->non_vboot_context = vboot;
		log_warning("flags %x %d\n", ctx->flags,
			    ((ctx->flags & VB2_CONTEXT_RECOVERY_MODE) != 0));
	} else {
		const struct sysinfo_t *sysinfo = cb_get_sysinfo();

		if (sysinfo) {
			const struct cb_mainboard *mb = sysinfo->mainboard;
			const char *ptr;

			ptr = (char *)mb->strings + mb->part_number_idx;
			if (mb->strings && strnlen(ptr, 30) < 30)
				log_notice("Starting vboot on %.30s...\n",
					   mb->strings + mb->part_number_idx);
// 			ret = vboot_make_context(sysinfo, &vboot->ctx);
// 			if (ret)
// 				return log_msg_ret("ctx", ret);
			vboot->from_coreboot = true;
			vboot->sysinfo = sysinfo;
		} else {
			log_err("No vboot handoff info\n");
			return -ENOENT;
		}
	}

	vboot->valid = true;

	ret = vboot_load_config(vboot);
	if (ret)
		return log_msg_ret("load", ret);

	ret = uclass_first_device_err(UCLASS_TPM, &vboot->tpm);
	if (ret)
		return log_msg_ret("tpm", ret);

	ret = uclass_first_device_err(UCLASS_CROS_FWSTORE, &vboot->fwstore);
	if (ret)
		return log_msg_ret("fwstore", ret);

	if (!vboot->from_coreboot) {
		ret = cros_ofnode_flashmap(&vboot->fmap);
		if (ret)
			return log_msg_ret("ofmap\n", ret);
	} else {
		ret = fmap_read(vboot);
		if (ret)
			return log_msg_ret("fmap\n", ret);
	}
	cros_ofnode_dump_fmap(&vboot->fmap);

	ret = vboot_keymap_init(vboot);
	if (ret)
		return log_msg_ret("key remap", ret);

	ret = cros_fwstore_read_entry(vboot->fwstore,
				      &vboot->fmap.readonly.firmware_id,
				      &vboot->readonly_firmware_id,
				      sizeof(vboot->readonly_firmware_id));
	if (ret)
		return log_msg_ret("ro", ret);

	if (vboot_is_slot_a(vboot))
		fw_entry = &vboot->fmap.readwrite_a;
	else
		fw_entry = &vboot->fmap.readwrite_b;
	ret = cros_fwstore_read_entry(vboot->fwstore,
				      &fw_entry->firmware_id,
				      &vboot->firmware_id,
				      sizeof(vboot->firmware_id));
	if (ret)
		return log_msg_ret("rw", ret);

#if CONFIG_IS_ENABLED(CROS_EC)
	ret = uclass_get_device(UCLASS_CROS_EC, 0, &vboot->cros_ec);
	if (ret)
		return log_msg_ret("ec", ret);
#endif
	ret = vboot_init_handoff(vboot);
	if (ret)
		return log_msg_ret("handoff", ret);

	return 0;
}
