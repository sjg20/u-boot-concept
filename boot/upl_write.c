// SPDX-License-Identifier: GPL-2.0+
/*
 * UPL handoff generation
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <log.h>
#include <upl.h>
#include <dm/ofnode.h>
#include "upl_common.h"

/**
 * write_addr() - Write an address
 *
 * Writes an address in the correct format, either 32- or 64-bit
 *
 * @upl: UPL state
 * @node: Node to write to
 * @prop: Property name to write
 * @addr: Address to write
 * Return: 0 if OK, -ve on error
 */
static int write_addr(const struct upl *upl, ofnode node, const char *prop,
		      ulong addr)
{
	int ret;

	if (upl->addr_cells == 1)
		ret = ofnode_write_u32(node, prop, addr);
	else
		ret = ofnode_write_u64(node, prop, addr);

	return ret;
}

/**
 * ofnode_write_bitmask() - Write a bit mask as a string list
 *
 * @node: Node to write to
 * @prop: Property name to write
 * @names: Array of names for each bit
 * @count: Number of array entries
 * @value: Bit-mask value to write
 * Return: 0 if OK, -EINVAL if a bit number is not defined, -ENOSPC if the
 * string is too long for the (internal) buffer
 */
static int ofnode_write_bitmask(ofnode node, const char *prop,
				const char *const names[], uint count,
				uint value)
{
	char buf[128];
	char *ptr, *end = buf + sizeof(buf);
	uint bit;
	int ret;

	ptr = buf;
	for (bit = 0; bit < count; bit++) {
		if (value & BIT(bit)) {
			const char *str = names[bit];
			uint len;

			if (!str) {
				log_debug("Unnamed bit number %d\n", bit);
				return log_msg_ret("bit", -EINVAL);
			}
			len = strlen(str) + 1;
			if (ptr + len > end) {
				log_debug("String array too long\n");
				return log_msg_ret("bit", -ENOSPC);
			}

			memcpy(ptr, str, len);
			ptr += len;
		}
	}

	ret = ofnode_write_prop(node, prop, buf, ptr - buf, true);
	if (ret)
		return log_msg_ret("wri", ret);

	return 0;
}

/**
 * ofnode_write_value() - Write an int as a string value using a lookup
 *
 * @node: Node to write to
 * @prop: Property name to write
 * @names: Array of names for each int value
 * @count: Number of array entries
 * @value: Int value to write
 * Return: 0 if OK, -EINVAL if a bit number is not defined, -ENOSPC if the
 * string is too long for the (internal) buffer
 */
static int ofnode_write_value(ofnode node, const char *prop,
			      const char *const names[], uint count,
			      uint value)
{
	const char *str;
	int ret;

	if (value >= count) {
		log_debug("Value of range %d\n", value);
		return log_msg_ret("val", -ERANGE);
	}
	str = names[value];
	if (!str) {
		log_debug("Unnamed value %d\n", value);
		return log_msg_ret("val", -EINVAL);
	}
	ret = ofnode_write_string(node, prop, str);
	if (ret)
		return log_msg_ret("wri", ret);

	return 0;
}

/**
 * encode_addr_size() - Write an address/size pair
 *
 * Writes an address and size into a buffer suitable for placing in a devicetree
 * 'reg' property. This uses upl->addr/size_cells to determine the number of
 * cells for each value
 *
 * @upl: UPL state
 * @buf: Buffer to write to
 * @size: Buffer size in bytes
 * @reg: Region to process
 * Returns: Number of bytes written, or -ENOSPC if the buffer is too small
 */
static int encode_addr_size(const struct upl *upl, char *buf, uint size,
			    const struct memregion *reg)
{
	char *ptr = buf;

	if (sizeof(fdt32_t) * (upl->addr_cells + upl->size_cells) > size)
		return log_msg_ret("eas", -ENOSPC);

	if (upl->addr_cells == 1)
		*(u32 *)ptr = cpu_to_fdt32(reg->base);
	else
		*(u64 *)ptr = cpu_to_fdt64(reg->base);
	ptr += upl->addr_cells * sizeof(u32);

	if (upl->size_cells == 1)
		*(u32 *)ptr = cpu_to_fdt32(reg->size);
	else
		*(u64 *)ptr = cpu_to_fdt64(reg->size);
	ptr += upl->size_cells * sizeof(u32);

	return ptr - buf;
}

/**
 * encode_reg() - Generate a set of addr/size pairs
 *
 * Each base/size value from each region is written to the buffer in a suitable
 * format to be written to the devicetree
 *
 * @upl: UPL state
 * @buf: Buffer to write to
 * @size: Buffer size
 * @num_regions: Number of regions to process
 * @region: List of regions to process (struct memregion)
 * Returns: Number of bytes written, or -ENOSPC if the buffer is too small
 */
static int encode_reg(const struct upl *upl, char *buf, int size,
		      uint num_regions, const struct alist *region)
{
	char *ptr, *end = buf + size;
	int i;

	ptr = buf;
	for (i = 0; i < num_regions; i++) {
		const struct memregion *reg = alist_get(region, i,
							struct memregion);
		int ret;

		ret = encode_addr_size(upl, ptr, end - ptr, reg);
		if (ret < 0)
			return log_msg_ret("uer", ret);
		ptr += ret;
	}

	return ptr - buf;
}

/**
 * add_addr_size_cells() - Add #address/#size-cells properties to the tree
 *
 * @node: Node to add to
 * Return 0 if OK, -ve on error
 */
static int add_addr_size_cells(ofnode node, int addr_cells, int size_cells)
{
	int ret;

	ret = ofnode_write_u32(node, UPLP_ADDRESS_CELLS, addr_cells);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_SIZE_CELLS, size_cells);
	if (ret)
		return log_msg_ret("cel", ret);

	return 0;
}

/**
 * add_upl_params() - Add UPL parameters node
 *
 * @upl: UPL state
 * @options: /options node to add to
 * Return 0 if OK, -ve on error
 */
static int add_upl_params(const struct upl *upl, ofnode options)
{
	ofnode node;
	int ret;

	ret = add_addr_size_cells(options, upl->addr_cells, upl->size_cells);
	if (ret)
		return log_msg_ret("upa", ret);
	ret = ofnode_add_subnode(options, UPLN_UPL_PARAMS, &node);
	if (ret)
		return log_msg_ret("img", ret);

	ret = ofnode_write_string(node, "compatible", UPLP_UPL_PARAMS_COMPAT);
	if (!ret)
		ret = write_addr(upl, node, UPLP_SMBIOS, upl->smbios);
	if (!ret)
		ret = write_addr(upl, node, UPLP_ACPI, upl->acpi);
	if (!ret && upl->bootmode)
		ret = ofnode_write_bitmask(node, UPLP_BOOTMODE, bootmode_names,
					   UPLBM_COUNT, upl->bootmode);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_ADDR_WIDTH, upl->addr_width);
	if (ret)
		return log_msg_ret("cnf", ret);

	return 0;
}

/**
 * add_upl_images() - Add /options/upl-images/upl-image nodes / props to tree
 *
 * @upl: UPL state
 * @node: /options node to add to
 * Return 0 if OK, -ve on error
 */
static int add_upl_images(const struct upl *upl, ofnode options)
{
	const struct upl_image *img;
	char name[40];
	ofnode node;
	int ret;

	snprintf(name, sizeof(name), UPLN_UPL_IMAGES "@%llx", upl->fit.base);
	ret = ofnode_add_subnode(options, name, &node);
	if (ret)
		return log_msg_ret("img", ret);

	if (upl->fit.base) {
		char buf[4 * sizeof(u64)];

		ret = encode_addr_size(upl, buf, sizeof(buf), &upl->fit);
		if (ret < 0)
			return log_msg_ret("uft", ret);
		ret = ofnode_write_prop(node, UPLP_REG, buf, ret, true);
		if (ret)
			return log_msg_ret("ufw", ret);
	}
	if (upl->conf_offset)
		ret = ofnode_write_u32(node, UPLP_CONF_OFFSET,
				       upl->conf_offset);
	if (ret)
		return log_msg_ret("cnf", ret);

	ret = add_addr_size_cells(node, upl->addr_cells, upl->size_cells);
	if (ret)
		return log_msg_ret("upi", ret);

	alist_for_each(img, &upl->image) {
		char buf[sizeof(u64) * 4];
		ofnode subnode;
		char name[30];
		int len;

		snprintf(name, sizeof(name), UPLN_IMAGE "@%llx", img->reg.base);
		ret = ofnode_add_subnode(node, name, &subnode);
		if (ret)
			return log_msg_ret("sub", ret);

		len = encode_addr_size(upl, buf, sizeof(buf), &img->reg);
		if (len < 0)
			return log_msg_ret("rbf", len);
		ret = ofnode_write_prop(subnode, UPLP_REG, buf, len, true);

		if (!ret && img->entry) {
			ret = write_addr(upl, subnode, UPLP_ENTRY, img->entry);
			if (ret < 0)
				return log_msg_ret("uwr", ret);
		}
		if (!ret && img->offset)
			ret = ofnode_write_u32(subnode, UPLP_OFFSET,
					       img->offset);
		if (!ret)
			ret = ofnode_write_string(subnode, UPLP_DESCRIPTION,
						  img->description);
		if (ret)
			return log_msg_ret("sim", ret);
	}

	return 0;
}

/**
 * write_mem_node() - Write a memory node and reg property
 *
 * Creates a new node and then adds a 'reg' property within it, listing each of
 * the memory regions in @mem
 *
 * @upl: UPL state
 * @parent: Parent node for the new node
 * @mem: List of memory regions to write (struct memregion)
 * @leaf: Name of memory node (so name is <leaf>@<unit_address>)
 * @nodep: Returns the created node
 * Returns 0 if OK, -ve on error
 */
static int write_mem_node(const struct upl *upl, ofnode parent,
			  const struct alist *mem, const char *leaf,
			  ofnode *nodep)
{
	char buf[mem->count * sizeof(u64) * 2];
	const struct memregion *first;
	char name[26];
	ofnode node;
	int ret, len;

	if (!mem->count) {
		log_debug("Memory '%s' has no regions\n", leaf);
		return log_msg_ret("reg", -EINVAL);
	}
	first = alist_get(mem, 0, struct memregion);
	sprintf(name, "%s@%llx", leaf, first->base);
	ret = ofnode_add_subnode(parent, name, &node);
	if (ret)
		return log_msg_ret("wmn", ret);

	len = encode_reg(upl, buf, sizeof(buf), mem->count, mem);
	ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
	if (ret)
		return log_msg_ret("wm1", ret);
	*nodep = node;

	return 0;
}

/**
 * add_upl_memory() - Add /memory nodes to the tree
 *
 * @upl: UPL state
 * @root: Parent node to contain the new /memory nodes
 * Return 0 if OK, -ve on error
 */
static int add_upl_memory(const struct upl *upl, ofnode root)
{
	const struct upl_mem *mem;

	alist_for_each(mem, &upl->mem) {
		ofnode node;
		int ret;

		ret = write_mem_node(upl, root, &mem->region, UPLN_MEMORY,
				     &node);
		if (ret)
			return log_msg_ret("ume", ret);

		if (mem->hotpluggable)
			ret = ofnode_write_bool(node, UPLP_HOTPLUGGABLE,
						mem->hotpluggable);
		if (ret)
			return log_msg_ret("lst", ret);
	}

	return 0;
}

/**
 * add_upl_memmap() - Add memory-map nodes to the tree
 *
 * @upl: UPL state
 * @root: Parent node to contain the new /memory-map node and its subnodes
 * Return 0 if OK, -ve on error
 */
static int add_upl_memmap(const struct upl *upl, ofnode root)
{
	const struct upl_memmap *memmap;
	ofnode mem_node;
	int ret;

	if (!upl->memmap.count)
		return 0;
	ret = ofnode_add_subnode(root, UPLN_MEMORY_MAP, &mem_node);
	if (ret)
		return log_msg_ret("img", ret);

	alist_for_each(memmap, &upl->memmap) {
		ofnode node;

		ret = write_mem_node(upl, mem_node, &memmap->region,
				     memmap->name, &node);
		if (ret)
			return log_msg_ret("umm", ret);
		if (memmap->usage) {
			ret = ofnode_write_bitmask(node, UPLP_USAGE,
						   usage_names,
						   UPLUS_COUNT, memmap->usage);
		}
		if (ret)
			return log_msg_ret("lst", ret);
	}

	return 0;
}

/**
 * add_upl_memres() - Add /reserved-memory nodes to the tree
 *
 * @upl: UPL state
 * @root: Parent node to contain the new node
 * Return 0 if OK, -ve on error
 */
static int add_upl_memres(const struct upl *upl, ofnode root,
			  bool skip_existing)
{
	const struct upl_memres *memres;
	ofnode mem_node;
	int ret;

	if (!upl->memres.count)
		return 0;
	ret = ofnode_add_subnode(root, UPLN_MEMORY_RESERVED, &mem_node);
	if (ret) {
		if (skip_existing && ret == -EEXIST)
			return 0;
		return log_msg_ret("img", ret);
	}
	ret = add_addr_size_cells(mem_node, upl->addr_cells, upl->size_cells);
	if (ret)
		return log_msg_ret("im2", ret);

	alist_for_each(memres, &upl->memres) {
		ofnode node;

		ret = write_mem_node(upl, mem_node, &memres->region,
				     memres->name, &node);
		if (ret)
			return log_msg_ret("umr", ret);

		if (memres->no_map)
			ret = ofnode_write_bool(node, UPLP_NO_MAP,
						memres->no_map);
		if (ret)
			return log_msg_ret("lst", ret);
	}

	return 0;
}

/**
 * add_upl_serial() - Add serial node
 *
 * @upl: UPL state
 * @root: Parent node to contain the new node
 * Return 0 if OK, -ve on error
 */
static int add_upl_serial(const struct upl *upl, ofnode root,
			  bool skip_existing)
{
	const struct upl_serial *ser = &upl->serial;
	const struct memregion *first;
	char name[26], alias[36];
	ofnode node, parent;
	int ret;

	if (!ser->compatible)
		return 0;
	if (!ser->reg.count)
		return log_msg_ret("ser", -EINVAL);

	ret = ofnode_add_subnode(root, UPLN_CHOSEN, &node);
	if (ret)
		return log_msg_ret("uch", ret);

	parent = root;
	if (ser->access_type == UPLAT_IO) {
		ret = ofnode_add_subnode(root, "isa", &node);
		if (ret)
			return log_msg_ret("uc1", ret);
		if (ofnode_write_string(node, UPLP_COMPATIBLE, "isa") ||
		    add_addr_size_cells(node, 2, 1))
			return log_msg_ret("uc2", -EINVAL);
		parent = node;
		strcpy(alias, "/isa");
	}

	first = alist_get(&ser->reg, 0, struct memregion);
	sprintf(name, UPLN_SERIAL "@%llx", first->base);
	ret = ofnode_add_subnode(parent, name, &node);
	if (ret) {
		if (ret == -EEXIST) {
			if (skip_existing)
				return 0;
		} else {
			return log_msg_ret("img", ret);
		}
	}

	ret = ofnode_write_string(node, UPLP_COMPATIBLE, ser->compatible);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_CLOCK_FREQUENCY,
				       ser->clock_frequency);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_CURRENT_SPEED,
				       ser->current_speed);
	if (!ret) {
		char buf[16];
		int len;

		len = encode_reg(upl, buf, sizeof(buf), 1, &ser->reg);
		if (len < 0)
			return log_msg_ret("aus", len);

		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
	}
	if (!ret && ser->reg_io_shift != UPLD_REG_IO_SHIFT)
		ret = ofnode_write_u32(node, UPLP_REG_IO_SHIFT,
				       ser->reg_io_shift);
	if (!ret && ser->reg_offset != UPLD_REG_OFFSET)
		ret = ofnode_write_u32(node, UPLP_REG_OFFSET, ser->reg_offset);
	if (!ret && ser->reg_io_width != UPLD_REG_IO_WIDTH)
		ret = ofnode_write_u32(node, UPLP_REG_IO_WIDTH,
				       ser->reg_io_width);
	if (!ret && ser->virtual_reg)
		ret = write_addr(upl, node, UPLP_VIRTUAL_REG, ser->virtual_reg);
	if (!ret) {
		ret = ofnode_write_value(node, UPLP_ACCESS_TYPE, access_types,
					 ARRAY_SIZE(access_types),
					 ser->access_type);
	}
	if (!ret) {
		strlcat(alias, "/", sizeof(alias));
		strlcat(alias, name, sizeof(alias));
		ret = ofnode_write_string(node, UPLP_STDOUT_PATH, alias);
	}
	if (ret)
		return log_msg_ret("ser", ret);

	return 0;
}

/**
 * add_upl_graphics() - Add graphics node
 *
 * @upl: UPL state
 * @root: Parent node to contain the new node
 * Return 0 if OK, -ve on error
 */
static int add_upl_graphics(const struct upl *upl, ofnode root)
{
	const struct upl_graphics *gra = &upl->graphics;
	const struct memregion *first;
	char name[36];
	ofnode node;
	int ret;

	if (!gra->reg.count)
		return log_msg_ret("ugr", -ENOENT);
	first = alist_get(&gra->reg, 0, struct memregion);
	sprintf(name, UPLN_GRAPHICS "@%llx", first->base);
	ret = ofnode_add_subnode(root, name, &node);
	if (ret)
		return log_msg_ret("gra", ret);

	ret = ofnode_write_string(node, UPLP_COMPATIBLE, UPLC_GRAPHICS);
	if (!ret) {
		char buf[16];
		int len;

		len = encode_reg(upl, buf, sizeof(buf), 1, &gra->reg);
		if (len < 0)
			return log_msg_ret("aug", len);

		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
	}
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_WIDTH, gra->width);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_HEIGHT, gra->height);
	if (!ret)
		ret = ofnode_write_u32(node, UPLP_STRIDE, gra->stride);
	if (!ret) {
		ret = ofnode_write_value(node, UPLP_GRAPHICS_FORMAT,
					 graphics_formats,
					 ARRAY_SIZE(graphics_formats),
					 gra->format);
	}
	if (ret)
		return log_msg_ret("pro", ret);

	return 0;
}

int upl_write_handoff(const struct upl *upl, ofnode root, bool skip_existing)
{
	ofnode options;
	int ret;

	ret = add_addr_size_cells(root, upl->addr_cells, upl->size_cells);
	if (ret)
		return log_msg_ret("ad1", ret);
	ret = ofnode_add_subnode(root, UPLN_OPTIONS, &options);
	if (ret && ret != -EEXIST)
		return log_msg_ret("opt", ret);

	ret = add_upl_params(upl, options);
	if (ret)
		return log_msg_ret("ad1", ret);

	ret = add_upl_images(upl, options);
	if (ret)
		return log_msg_ret("ad2", ret);

	ret = add_upl_memory(upl, root);
	if (ret)
		return log_msg_ret("ad3", ret);

	ret = add_upl_memmap(upl, root);
	if (ret)
		return log_msg_ret("ad4", ret);

	ret = add_upl_memres(upl, root, skip_existing);
	if (ret)
		return log_msg_ret("ad5", ret);

	ret = add_upl_serial(upl, root, skip_existing);
	if (ret)
		return log_msg_ret("ad6", ret);

	ret = add_upl_graphics(upl, root);
	if (ret && ret != -ENOENT)
		return log_msg_ret("ad7", ret);

	return 0;
}

int upl_create_handoff_tree(const struct upl *upl, oftree *treep)
{
	ofnode root;
	oftree tree;
	int ret;

	ret = oftree_new(&tree);
	if (ret)
		return log_msg_ret("cht", -EINVAL);

	root = oftree_root(tree);
	if (!ofnode_valid(root))
		return log_msg_ret("roo", -EINVAL);

	ret = upl_write_handoff(upl, root, false);
	if (ret)
		return log_msg_ret("wr", ret);

	*treep = tree;

	return 0;
}

int upl_create_handoff(struct upl *upl, ulong addr, struct abuf *buf)
{
	oftree tree;
	int ret;

	ret = upl_create(upl);
	if (ret) {
		log_debug("Failed to create handoff (err=%dE)\n", ret);
		return log_msg_ret("cho", ret);
	}
	log_debug("2a images %d\n", upl->image.count);

	ret = oftree_new(&tree);
	if (ret)
		return log_msg_ret("new", ret);

	ret = upl_write_handoff(upl, oftree_root(tree), true);
	if (ret)
		return log_msg_ret("wr", ret);

	ret = oftree_to_fdt(tree, buf);
	if (ret)
		return log_msg_ret("fdt", ret);
	oftree_dispose(tree);

	return 0;
}
