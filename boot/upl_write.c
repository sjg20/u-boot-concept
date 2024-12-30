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

static int write_addr_size(const struct upl *upl, const struct memregion *reg,
			   char *buf, char *end)
{
	char *ptr = buf;

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
	if (ptr > end)
		return log_msg_ret("was", -ENOSPC);

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

	ret = ofnode_add_subnode(options, UPLN_UPL_PARAMS, &node);
	if (ret)
		return log_msg_ret("img", ret);

	ret = ofnode_write_string(node, "compatible", UPLP_UPL_PARAMS_COMPAT);
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
 * add_upl_image() - Add /options/upl-image nodes and properties to the tree
 *
 * @upl: UPL state
 * @node: /options node to add to
 * Return 0 if OK, -ve on error
 */
static int add_upl_image(const struct upl *upl, ofnode options)
{
	const struct upl_image *img;
	ofnode node;
	int ret;

	ret = ofnode_add_subnode(options, UPLN_UPL_IMAGE, &node);
	if (ret)
		return log_msg_ret("img", ret);

	if (upl->fit)
		ret = ofnode_write_u32(node, UPLP_FIT, upl->fit);
	if (!ret && upl->conf_offset)
		ret = ofnode_write_u32(node, UPLP_CONF_OFFSET,
				       upl->conf_offset);
	if (ret)
		return log_msg_ret("cnf", ret);

	alist_for_each(img, &upl->image) {
		char buf[sizeof(u64) * 4];
		struct memregion reg;
		ofnode subnode;
		char name[30];
		int len;

		snprintf(name, sizeof(name), UPLN_IMAGE "@%lx", img->load);
		ret = ofnode_add_subnode(node, name, &subnode);
		if (ret)
			return log_msg_ret("sub", ret);

		reg.base = img->load;
		reg.size = img->size;
		len = write_addr_size(upl, &reg, buf, buf + sizeof(buf));
		if (len < 0)
			return log_msg_ret("rbf", len);
		ret = ofnode_write_prop(subnode, UPLP_REG, buf, len, true);

		if (!ret && img->entry)
			ret = write_addr(upl, subnode, FIT_DATA_ENTRY, img->entry);
		if (!ret)
			ret = ofnode_write_string(subnode, UPLP_DESCRIPTION,
						  img->description);
		if (ret)
			return log_msg_ret("sim", ret);
	}

	return 0;
}

/**
 * buffer_addr_size() - Generate a set of addr/size pairs
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
static int buffer_addr_size(const struct upl *upl, char *buf, int size,
			    uint num_regions, const struct alist *region)
{
	char *ptr, *end = buf + size;
	const struct memregion *reg;

	ptr = buf;
	alist_for_each(reg, region) {
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
		if (ptr > end)
			return -ENOSPC;
	}

	return ptr - buf;
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
		char buf[mem->region.count * sizeof(u64) * 2];
		const struct memregion *first;
		char name[26];
		int ret, len;
		ofnode node;

		if (!mem->region.count) {
			log_debug("Memory has no regions\n");
			return log_msg_ret("reg", -EINVAL);
		}
		first = alist_get(&mem->region, 0, struct memregion);
		sprintf(name, UPLN_MEMORY "@%llx", first->base);
		ret = ofnode_add_subnode(root, name, &node);
		if (ret)
			return log_msg_ret("mem", ret);

		len = buffer_addr_size(upl, buf, sizeof(buf), mem->region.count,
				       &mem->region);
		if (len < 0)
			return log_msg_ret("buf", len);

		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
		if (!ret && mem->hotpluggable)
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
		char buf[memmap->region.count * sizeof(u64) * 2];
		const struct memregion *first;
		char name[26];
		int ret, len;
		ofnode node;

		if (!memmap->region.count) {
			log_debug("Memory-map has no regions\n");
			return log_msg_ret("reg", -EINVAL);
		}
		first = alist_get(&memmap->region, 0, struct memregion);
		sprintf(name, "%s@%llx", memmap->name, first->base);
		ret = ofnode_add_subnode(mem_node, name, &node);
		if (ret)
			return log_msg_ret("memmap", ret);

		len = buffer_addr_size(upl, buf, sizeof(buf),
				       memmap->region.count, &memmap->region);
		if (len < 0)
			return log_msg_ret("buf", len);
		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
		if (!ret && memmap->usage)
			ret = ofnode_write_bitmask(node, UPLP_USAGE,
						   usage_names,
						   UPLUS_COUNT, memmap->usage);
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
		char buf[memres->region.count * sizeof(u64) * 2];
		const struct memregion *first;
		char name[26];
		int ret, len;
		ofnode node;

		if (!memres->region.count) {
			log_debug("Memory-reserved has no regions\n");
			return log_msg_ret("reg", -EINVAL);
		}
		first = alist_get(&memres->region, 0, struct memregion);
		sprintf(name, "memory@%llx", first->base);
		ret = ofnode_add_subnode(mem_node, name, &node);
		if (ret)
			return log_msg_ret("memres", ret);

		if (memres->compat) {
			ret = ofnode_write_string(node, UPLP_COMPATIBLE,
						  memres->compat);
			if (ret)
				return log_msg_ret("mrc", ret);
		}

		len = buffer_addr_size(upl, buf, sizeof(buf),
				       memres->region.count, &memres->region);
		ret = ofnode_write_prop(node, UPLP_REG, buf, len, true);
		if (!ret && memres->no_map)
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
	char name[26];
	ofnode node;
	int ret;

	if (!ser->compatible || skip_existing)
		return 0;
	if (!ser->reg.count)
		return log_msg_ret("ser", -EINVAL);
	first = alist_get(&ser->reg, 0, struct memregion);
	sprintf(name, UPLN_SERIAL "@%llx", first->base);
	ret = ofnode_add_subnode(root, name, &node);
	if (ret)
		return log_msg_ret("img", ret);
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

		len = buffer_addr_size(upl, buf, sizeof(buf), 1, &ser->reg);
		if (len < 0)
			return log_msg_ret("buf", len);

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
		return log_msg_ret("gra", -ENOENT);
	first = alist_get(&gra->reg, 0, struct memregion);
	sprintf(name, UPLN_GRAPHICS "@%llx", first->base);
	ret = ofnode_add_subnode(root, name, &node);
	if (ret)
		return log_msg_ret("gra", ret);

	ret = ofnode_write_string(node, UPLP_COMPATIBLE, UPLC_GRAPHICS);
	if (!ret) {
		char buf[16];
		int len;

		len = buffer_addr_size(upl, buf, sizeof(buf), 1, &gra->reg);
		if (len < 0)
			return log_msg_ret("buf", len);

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
		return log_msg_ret("opt", -EINVAL);

	ret = add_upl_params(upl, options);
	if (ret)
		return log_msg_ret("ad1", ret);

	ret = add_upl_image(upl, options);
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
		return log_msg_ret("ad6", ret);

	return 0;
}

int upl_create_handoff_tree(const struct upl *upl, oftree *treep)
{
	ofnode root;
	oftree tree;
	int ret;

	ret = oftree_new(&tree);
	if (ret)
		return log_msg_ret("new", ret);

	root = oftree_root(tree);
	if (!ofnode_valid(root))
		return log_msg_ret("roo", -EINVAL);

	ret = upl_write_handoff(upl, root, false);
	if (ret)
		return log_msg_ret("wr", ret);

	*treep = tree;

	return 0;
}

int upl_create_handoff(struct upl *upl, struct abuf *buf)
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
