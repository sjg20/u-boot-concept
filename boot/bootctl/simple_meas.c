// SPDX-License-Identifier: GPL-2.0+
/*
 * Simple control of which display/output to use while booting
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#define LOG_CATEGORY	UCLASS_BOOTCTL

#include <bloblist.h>
#include <dm.h>
#include <tpm_api.h>
#include <tpm_tcg2.h>
#include <version_string.h>
#include <bootctl/measure.h>
#include <bootctl/oslist.h>
#include <linux/sizes.h>

enum {
	/* align TPM log to 4K boundary */
	ALIGN_LOG2	= 12,
};

/**
 * enum meas_algo_t - Available algorithms
 *
 * @ALGOT_SHA256: SHA256
 * @ALGOT_COUNT: Number of algorithms
 */
enum meas_algo_t {
	ALGOT_SHA256,

	ALGOT_COUNT,
};

/**
 * enum meas_payload_t - Types of things we can measure
 *
 * @PAYLOADT_OS: Operating system
 * @PAYLOADT_INITRD: Initial ramdisk
 * @PAYLOADT_FDT: Flattened devicetree
 * @PAYLOADT_CMDLINE: OS command line
 * @PAYLOADT_COUNT: Number of payload types
 */
enum meas_payload_t {
	PAYLOADT_OS,
	PAYLOADT_INITRD,
	PAYLOADT_FDT,
	PAYLOADT_CMDLINE,

	PAYLOADT_COUNT,
};

/**
 * struct meas_step - An individual measurement, e.g. for a single image
 *
 * For now only tpm-pcr is supported, so there is no field for the method - it
 * is assumed to be tpm-pcr
 *
 * These parameters are read from the devicetree.
 *
 * @payload_type: Image type to measure
 * @algos: Bitmap of algorithms to use to measure in this step
 * @pcr: TPM Platform Configuration Register to use for this step
 * @optional: true if it is OK if the image is missing and cannot be measured
 */
struct meas_step {
	enum meas_payload_t type;
	uint algos;
	uint pcr;
	bool optional;
};

/**
 * struct measure_priv - Private information for the measure driver
 *
 * @tpm: TPM to use for measurement
 * @tpm_log_size: Configured of TPM log
 * @elog: Information about TPM log
 * @steps: List of measurement steps, each struct measure_step
 */
struct measure_priv {
	struct udevice *tpm;
	u32 tpm_log_size;
	struct tcg2_event_log elog;
	struct alist steps;
};

static const char *const algo_name[ALGOT_COUNT] = {
	[ALGOT_SHA256] = "sha256"
};

static struct payload {
	const char *name;
	enum bootflow_img_t type;
} payload_info[PAYLOADT_COUNT] = {
	[PAYLOADT_OS]		= {"os", (enum bootflow_img_t)IH_TYPE_KERNEL},
	[PAYLOADT_INITRD]	= {"initrd",
				         (enum bootflow_img_t)IH_TYPE_RAMDISK},
	[PAYLOADT_FDT]		= {"fdt", (enum bootflow_img_t)IH_TYPE_FLATDT},
	[PAYLOADT_CMDLINE]	= {"cmdline", BFI_CMDLINE},
};

static int simple_start(struct udevice *dev)
{
	struct measure_priv *priv = dev_get_priv(dev);
	struct tcg2_event_log *elog = &priv->elog;
	struct udevice *tpm = priv->tpm;
	int ret, size;
	void *blob;

	if (!IS_ENABLED(CONFIG_TPM_V2))
		return log_msg_ret("spt", -ENOSYS);

	blob = bloblist_get_blob(BLOBLISTT_TPM_EVLOG, &size);
	if (blob) {
		if (size < priv->tpm_log_size)
			return log_msg_ret("spb", -ENOBUFS);

		/* we don't support changing the alignment at present */
		if ((ulong)blob != ALIGN((ulong)blob, 1 << ALIGN_LOG2))
			return log_msg_ret("spf", -EBADF);

		ret = bloblist_resize(BLOBLISTT_TPM_EVLOG, priv->tpm_log_size);
		if (ret)
			return log_msg_ret("msr", ret);
	} else {
		blob = bloblist_add(BLOBLISTT_TPM_EVLOG, priv->tpm_log_size,
				    ALIGN_LOG2);
		if (!blob)
			return log_msg_ret("sps", -ENOSPC);
	}

	ret = tpm_auto_start(tpm);
	if (ret)
		return log_msg_ret("spa", ret);

	elog->log = blob;
	elog->log_size = priv->tpm_log_size;
	ret = tcg2_log_init(tpm, elog);
	if (ret)
		return log_msg_ret("spi", ret);

	ret = tcg2_measure_event(tpm, elog, 0, EV_S_CRTM_VERSION,
				 strlen(version_string) + 1, version_string);
	if (ret) {
		tcg2_measurement_term(tpm, elog, true);
		return log_msg_ret("spe", ret);
	}

	return 0;
}

static const char *get_typename(enum bootflow_img_t type)
{
	switch ((int)type) {
	case IH_TYPE_KERNEL:
		return "linux";
	case IH_TYPE_RAMDISK:
		return "initrd";
	case IH_TYPE_FLATDT:
		return "dts";
	default:
		return NULL;
	}
}

static int simple_process(struct udevice *dev, const struct osinfo *osinfo,
			  struct alist *result)
{
	const struct bootflow *bflow = &osinfo->bflow;
	struct measure_priv *priv = dev_get_priv(dev);
	struct tcg2_event_log *elog = &priv->elog;
	const struct meas_step *step;

	if (!IS_ENABLED(CONFIG_TPM_V2))
		return log_msg_ret("ptp", -ENOSYS);

	alist_init_struct(result, struct measure_info);
	alist_for_each(step, &priv->steps) {
		enum bootflow_img_t type = payload_info[step->type].type;
		const struct bootflow_img *img;
		struct measure_info info;
		const char *typename;
		const void *ptr;
		int ret;

		log_debug("measuring %s\n", payload_info[step->type].name);
		img = bootflow_img_find(bflow, type);
		if (!img) {
			if (step->optional)
				continue;
			log_err("Missing image '%s'\n",
				bootflow_img_type_name(type));
			return log_msg_ret("smi", -ENOENT);
		}

		ptr = map_sysmem(img->addr, img->size);
		typename = get_typename(img->type);
		if (!typename) {
			log_err("Unknown image type %d (%s)\n", img->type,
				bootflow_img_type_name(img->type));
			return log_msg_ret("pim", -EINVAL);
		}

		/*
		 * TODO: Use the requested algos or at least check that the TPM
		 * supports them
		 *
		 * tcg2_measure_data() actually measures with the algorithms
		 * determined by the TPM. It also does this silently, so we
		 * don't really know what it did.
		 *
		 * See tcg2_get_pcr_info() for where this information is
		 * collected. We should add a function to the API that lets us
		 * see what is active
		 *
		 * Really the TPM code this should be integrated with the hash.h
		 * code too, rather than having parallel tables.
		 */
		ret = tcg2_measure_data(priv->tpm, elog, 8, img->size, ptr,
					EV_COMPACT_HASH, strlen(typename) + 1
					, typename);
		if (ret)
			return log_msg_ret("stc", ret);
		log_debug("Measured '%s'\n", bootflow_img_type_name(type));

		info.img = img;
		if (!alist_add(result, img))
			return log_msg_ret("sra", -ENOMEM);
	}

	return 0;
}

static int measure_probe(struct udevice *dev)
{
	struct measure_priv *priv = dev_get_priv(dev);
	struct udevice *tpm;

	if (!IS_ENABLED(CONFIG_TPM_V2))
		return log_msg_ret("spT", -ENOSYS);

	/*
	 * TODO: We have to probe TPMs to find out what version they are. This
	 * could be updated to happen in the bind() method of the TPM
	 */
	uclass_foreach_dev_probe(UCLASS_TPM, tpm) {
		if (tpm_get_version(tpm) == TPM_V2) {
			priv->tpm = tpm;
			break;
		}
	}

	/* TODO: Add policy for what to do in this case */
	if (!priv->tpm)
		log_warning("TPM not present\n");

	return 0;
}

static int measure_of_to_plat(struct udevice *dev)
{
	struct measure_priv *priv = dev_get_priv(dev);
	ofnode node;

	alist_init_struct(&priv->steps, struct meas_step);

	priv->tpm_log_size = SZ_64K;
	dev_read_u32(dev, "tpm-log-size", &priv->tpm_log_size);

	/* errors should not happen in production code, so use log_debug() */
	for (node = ofnode_first_subnode(dev_ofnode(dev)); ofnode_valid(node);
	     node = ofnode_next_subnode(node)) {
		const char *node_name = ofnode_get_name(node);
		const char *method = ofnode_read_string(node, "method");
		struct meas_step step = {0};
		int count, i, j, ret, found;

		/* for now we use the node name as the payload name */
		for (j = 0, found = -1; j < ARRAY_SIZE(payload_info); j++) {
			if (!strcmp(payload_info[j].name, node_name))
				found = j;
		}
		if (found == -1) {
			log_debug("Unknown payload '%s'\n", node_name);
			return log_msg_ret("mta", -EINVAL);
		}
		step.type = found;
		step.optional = ofnode_read_bool(node, "optional");

		if (!method || strcmp("tpm-pcr", method)) {
			log_debug("Unknown method in '%s': '%s'\n", node_name,
				  method);
			return log_msg_ret("mtp", -EINVAL);
		}

		ret = ofnode_read_u32(node, "pcr-number", &step.pcr);
		if (ret) {
			log_debug("Missing pcr-number in '%s'", node_name);
			return log_msg_ret("mtP", ret);
		}

		count = ofnode_read_string_count(node, "algos");
		if (count < 1) {
			log_debug("Missing algos in '%s'\n", node_name);
			return log_msg_ret("mta", -EINVAL);
		}

		for (i = 0; i < count; i++) {
			const char *name;

			ret = ofnode_read_string_index(node, "algos", i, &name);
			if (ret)
				return log_msg_ret("mta", ret);
			for (j = 0, found = -1;
			     j < ARRAY_SIZE(algo_name); j++) {
				if (!strcmp(algo_name[j], name))
					found = j;
			}
			if (found == -1) {
				log_debug("Unknown algo in '%s': '%s'\n",
					  node_name, name);
				return log_msg_ret("mta", -EINVAL);
			}
			step.algos |= BIT(found);
		}

		if (!alist_add(&priv->steps, step))
			return log_msg_ret("mal", -ENOMEM);
	}

	return 0;
}

static struct bc_measure_ops measure_ops = {
	.start		= simple_start,
	.process	= simple_process,
};

static const struct udevice_id measure_ids[] = {
	{ .compatible = "bootctl,simple-measure" },
	{ .compatible = "bootctl,measure" },
	{ }
};

U_BOOT_DRIVER(simple_measure) = {
	.name		= "simple_meas",
	.id		= UCLASS_BOOTCTL_MEASURE,
	.of_match	= measure_ids,
	.ops		= &measure_ops,
	.priv_auto	= sizeof(struct measure_priv),
	.of_to_plat	= measure_of_to_plat,
	.probe		= measure_probe,
};
