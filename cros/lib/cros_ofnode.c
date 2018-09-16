/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <common.h>
#include <errno.h>
#include <fdtdec.h>
#include <log.h>
#include <malloc.h>
#include <asm/io.h>
#include <cros/cros_common.h>
#include <cros/cros_ofnode.h>
#include <cros/fmap.h>
#include <linux/string.h>

/*
 * Some platforms where DRAM is based at zero do not define DRAM base address
 * explicitly.
 */
#ifdef CONFIG_SYS_SDRAM_BASE
#define DRAM_BASE_ADDRESS CONFIG_SYS_SDRAM_BASE
#else
#define DRAM_BASE_ADDRESS 0
#endif

ofnode cros_ofnode_config_node(void)
{
	ofnode node = ofnode_path("/chromeos-config");

	if (!ofnode_valid(node))
		VB2_DEBUG("failed to find /chromeos-config\n");

	return node;
}

/* These are the various flashmap nodes that we are interested in */
enum section_t {
	SECTION_BASE,		/* group section, no name: rw-a and rw-b */
	SECTION_FIRMWARE_ID,
	SECTION_BOOT,
	SECTION_GBB,
	SECTION_VBLOCK,
	SECTION_FMAP,
	SECTION_ECRW,
	SECTION_ECRO,
	SECTION_PDRW,
	SECTION_PDRO,
	SECTION_SPL,
	SECTION_BOOT_REC,
	SECTION_SPL_REC,

	SECTION_COUNT,
	SECTION_NONE = -1,
};

/* Names for each section, preceeded by ro-, rw-a- or rw-b- */
static const char *section_name[SECTION_COUNT] = {
	"",
	"firmware-id",
	"boot",
	"gbb",
	"vblock",
	"fmap",
	"ecrw",
	"ecro",
	"pdrw",
	"pdro",
	"spl",
	"boot-rec",
	"spl-rec",
};

/**
 * Look up a section name and return its type
 *
 * @param name		Name of section (after ro- or rw-a/b- part)
 * @return section type section_t, or SECTION_NONE if none
 */
static enum section_t lookup_section(const char *name)
{
	char *at;
	int i, len;

	at = strchr(name, '@');
	len = at ? at - name : strlen(name);
	for (i = 0; i < SECTION_COUNT; i++)
		if (0 == strncmp(name, section_name[i], len))
			return i;

	return SECTION_NONE;
}

/**
 * Process a flashmap node, storing its information in our config.
 *
 * @param node		ofnode to read
 * @param config	Place to put the information we read
 *
 * Both rwp and ecp start as NULL and are updated when we see an RW and an
 * EC region respectively. This function is called for every node in the
 * device tree and these variables maintain the state that we need to
 * process the nodes correctly.
 *
 * @return 0 if ok, -ve on error
 */
static int process_fmap_node(ofnode node, struct twostop_fmap *config,
			     struct fmap_firmware_entry *fw)
{
	enum section_t section;
	struct fmap_entry *entry;
	const char *name;
	int ret;

	name = ofnode_get_name(node);
	if (!strcmp("rw-vblock-dev", name))
		return log_msg_ret("rw-vblock-dev",
				   ofnode_read_fmap_entry(node,
						&config->readwrite_devkey));

	if (!strcmp("rw-elog", name))
		return log_msg_ret("rw-elog",
				   ofnode_read_fmap_entry(node, &config->elog));

	section = lookup_section(name);
	VB2_DEBUG("lookup_section '%s': %d\n", name, section);
	entry = NULL;

	/*
	 * TODO(sjg@chromium.org): We could use offsetof() here and avoid
	 * this switch by putting the offset of each field in a table.
	 */
	switch (section) {
	case SECTION_BASE:
		entry = &fw->all;
		fw->block_offset = ofnode_read_u64_default(node, "block-offset",
							   ~0ULL);
		if (fw->block_offset == ~0ULL)
			VB2_DEBUG("Node '%s': bad block-offset\n", name);
		break;
	case SECTION_FIRMWARE_ID:
		entry = &fw->firmware_id;
		break;
	case SECTION_BOOT:
		entry = &fw->boot;
		break;
	case SECTION_GBB:
		entry = &fw->gbb;
		break;
	case SECTION_VBLOCK:
		entry = &fw->vblock;
		break;
	case SECTION_FMAP:
		entry = &fw->fmap;
		break;
	case SECTION_ECRW:
		entry = &fw->ec[EC_MAIN].rw;
		break;
	case SECTION_ECRO:
		entry = &fw->ec[EC_MAIN].ro;
		break;
	case SECTION_PDRW:
		entry = &fw->ec[EC_PD].rw;
		break;
	case SECTION_PDRO:
		entry = &fw->ec[EC_PD].ro;
		break;
	case SECTION_SPL:
		entry = &fw->spl;
		break;
	case SECTION_BOOT_REC:
		entry = &fw->boot_rec;
		break;
	case SECTION_SPL_REC:
		entry = &fw->spl_rec;
		break;
	case SECTION_COUNT:
	case SECTION_NONE:
		return 0;
	}

	/* Read in the properties */
	assert(entry);
	if (entry) {
		ret = ofnode_read_fmap_entry(node, entry);
		if (ret)
			return log_msg_ret(ofnode_get_name(node), ret);
	}

	return 0;
}

int cros_ofnode_flashmap(struct twostop_fmap *config)
{
	struct fmap_entry entry;
	ofnode node;
	int ret;

	memset(config, '\0', sizeof(*config));
	node = ofnode_by_compatible(ofnode_null(), "chromeos,flashmap");
	if (!ofnode_valid(node))
		return log_msg_ret("chromeos,flashmap node is missing",
				   -EINVAL);

	/* Read in the 'reg' property */
	if (ofnode_read_fmap_entry(node, &entry))
		return log_ret(-EINVAL);
	config->flash_base = entry.offset;

	ofnode_for_each_subnode(node, node) {
		struct fmap_firmware_entry *fw;
		const char *name;
		ofnode subnode;

		name = ofnode_get_name(node);
		if (strlen(name) < 5) {
			VB2_DEBUG("Node name '%s' is too short\n");
			return log_ret(-EINVAL);
		}
		fw = NULL;
		if (!strcmp(name, "read-only")) {
			fw = &config->readonly;
		} else if (!strcmp(name, "read-write-a")) {
			fw = &config->readwrite_a;
		} else if (!strcmp(name, "read-write-b")) {
			fw = &config->readwrite_b;
		} else {
			VB2_DEBUG("Ignoring section '%s'\n", name);
			continue;
		}
		ofnode_for_each_subnode(subnode, node) {
			ret = process_fmap_node(subnode, config, fw);
			if (ret)
				return log_msg_ret("Failed to process Flashmap",
						   -EINVAL);
		}
		printf("no more subnodes\n");
	}

	return 0;
}

int cros_ofnode_find_locale(const char *name, struct fmap_entry *entry)
{
	ofnode node, subnode;
	int ret;

	node = ofnode_by_compatible(ofnode_null(), "chromeos,locales");
	if (!ofnode_valid(node))
		return log_msg_ret("chromeos,locales node is missing",
				   -EINVAL);
	subnode = ofnode_find_subnode(node, name);
	if (!ofnode_valid(subnode)) {
		log(LOGC_VBOOT, LOGL_ERR, "Locale not found: %s\n", name);
		return log_msg_ret("Locale not found", -ENOENT);
	}
	ret = ofnode_read_fmap_entry(subnode, entry);
	if (ret)
		return log_msg_ret(ofnode_get_name(subnode), ret);

	return 0;
}

int cros_ofnode_config_has_prop(const void *blob, const char *name)
{
	ofnode node = cros_ofnode_config_node();

	return ofnode_valid(node) &&
		ofnode_get_property(node, name, NULL);
}

int cros_ofnode_decode_region(const char *mem_type, const char *suffix,
			      fdt_addr_t *basep, fdt_size_t *sizep)
{
	ofnode node = cros_ofnode_config_node();
	int ret;

	if (!ofnode_valid(node))
		return -ENOENT;
	ret = ofnode_decode_memory_region(node, mem_type, suffix, basep, sizep);
	if (ret) {
		VB2_DEBUG("failed to find %s suffix %s in /chromeos-config\n",
			mem_type, suffix);
		return ret;
	}

	return 0;
}

int cros_ofnode_memory(const char *name, struct fdt_memory *config)
{
	const fdt_addr_t *cell;
	ofnode node;
	int len;

	node = ofnode_path(name);
	if (!ofnode_valid(node))
		return -EINVAL;

	cell = ofnode_get_property(node, "reg", &len);
	if (cell && len >= sizeof(fdt_addr_t) * 2) {
		config->start = fdt_addr_to_cpu(cell[0]);
		config->end = config->start + fdt_addr_to_cpu(cell[1]);
	} else
		return -FDT_ERR_BADLAYOUT;

	return 0;
}
