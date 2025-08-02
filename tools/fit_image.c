// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2008 Semihalf
 *
 * (C) Copyright 2000-2004
 * DENX Software Engineering
 * Wolfgang Denk, wd@denx.de
 *
 * Updated-by: Prafulla Wadaskar <prafulla@marvell.com>
 *		FIT image specific code abstracted from mkimage.c
 *		some functions added to address abstraction
 *
 * All rights reserved.
 */

#include "imagetool.h"
#include "fit_common.h"
#include "mkimage.h"
#include <image.h>
#include <string.h>
#include <stdarg.h>
#include <version.h>
#include <u-boot/crc.h>

static struct legacy_img_hdr header;

static int fit_add_file_data(struct imgtool *itl, size_t size_inc,
			     const char *tmpfile)
{
	int tfd, destfd = 0;
	void *dest_blob = NULL;
	off_t destfd_size = 0;
	struct stat sbuf;
	void *ptr;
	int ret = 0;

	tfd = mmap_fdt(itl->cmdname, tmpfile, size_inc, &ptr, &sbuf, true,
		       false);
	if (tfd < 0) {
		fprintf(stderr, "Cannot map FDT file '%s'\n", tmpfile);
		return -EIO;
	}

	if (itl->keydest) {
		struct stat dest_sbuf;

		destfd = mmap_fdt(itl->cmdname, itl->keydest, size_inc,
				  &dest_blob, &dest_sbuf, false,
				  false);
		if (destfd < 0) {
			ret = -EIO;
			goto err_keydest;
		}
		destfd_size = dest_sbuf.st_size;
	}

	/* for first image creation, add a timestamp at offset 0 i.e., root  */
	if (itl->datafile || itl->reset_timestamp) {
		time_t time = imagetool_get_source_date(itl->cmdname,
							sbuf.st_mtime);
		ret = fit_set_timestamp(ptr, 0, time);
	}

	if (CONFIG_IS_ENABLED(FIT_SIGNATURE) && !ret)
		ret = fit_pre_load_data(itl->keydir, dest_blob, ptr);

	if (!ret) {
		ret = fit_cipher_data(itl->keydir, dest_blob, ptr,
				      itl->comment,
				      itl->require_keys,
				      itl->engine_id,
				      itl->cmdname);
	}

	if (!ret) {
		ret = fit_add_verification_data(itl->keydir,
						itl->keyfile, dest_blob, ptr,
						itl->comment,
						itl->require_keys,
						itl->engine_id,
						itl->cmdname,
						itl->algo_name,
						&itl->summary);
	}

	if (dest_blob) {
		munmap(dest_blob, destfd_size);
		close(destfd);
	}

err_keydest:
	munmap(ptr, sbuf.st_size);
	close(tfd);
	return ret;
}

/**
 * fit_calc_size() - Calculate the approximate size of the FIT we will generate
 */
static int fit_calc_size(struct imgtool *itl)
{
	struct content_info *cont;
	int size, total_size;

	size = imagetool_get_filesize(itl, itl->datafile);
	if (size < 0)
		return -1;
	total_size = size;

	if (itl->fit_ramdisk) {
		size = imagetool_get_filesize(itl, itl->fit_ramdisk);
		if (size < 0)
			return -1;
		total_size += size;
	}

	for (cont = itl->content_head; cont; cont = cont->next) {
		size = imagetool_get_filesize(itl, cont->fname);
		if (size < 0)
			return -1;

		/* Add space for properties and hash node */
		total_size += size + 300;
	}

	/* Add plenty of space for headers, properties, nodes, etc. */
	total_size += 4096;

	return total_size;
}

static int fdt_property_file(struct imgtool *itl, void *fdt, const char *name,
			     const char *fname)
{
	struct stat sbuf;
	void *ptr;
	int ret;
	int fd;

	fd = open(fname, O_RDONLY | O_BINARY);
	if (fd < 0) {
		fprintf(stderr, "%s: Can't open %s: %s\n",
			itl->cmdname, fname, strerror(errno));
		return -1;
	}

	if (fstat(fd, &sbuf) < 0) {
		fprintf(stderr, "%s: Can't stat %s: %s\n",
			itl->cmdname, fname, strerror(errno));
		goto err;
	}

	ret = fdt_property_placeholder(fdt, "data", sbuf.st_size, &ptr);
	if (ret)
		goto err;
	ret = read(fd, ptr, sbuf.st_size);
	if (ret != sbuf.st_size) {
		fprintf(stderr, "%s: Can't read %s: %s\n",
			itl->cmdname, fname, strerror(errno));
		goto err;
	}
	close(fd);

	return 0;
err:
	close(fd);
	return -1;
}

static int fdt_property_strf(void *fdt, const char *name, const char *fmt, ...)
{
	char str[100];
	va_list ptr;

	va_start(ptr, fmt);
	vsnprintf(str, sizeof(str), fmt, ptr);
	va_end(ptr);
	return fdt_property_string(fdt, name, str);
}

static void get_basename(char *str, int size, const char *fname)
{
	const char *p, *start, *end;
	int len;

	/*
	 * Use the base name as the 'name' field. So for example:
	 *
	 * "arch/arm/dts/sun7i-a20-bananapro.dtb"
	 * becomes "sun7i-a20-bananapro"
	 */
	p = strrchr(fname, '/');
	start = p ? p + 1 : fname;
	p = strrchr(fname, '.');
	end = p ? p : fname + strlen(fname);
	len = end - start;
	if (len >= size)
		len = size - 1;
	memcpy(str, start, len);
	str[len] = '\0';
}

/**
 * fit_add_hash_or_sign() - Add a hash or signature node
 *
 * @itl: Image-tool info
 * @fdt: Device tree to add to (in sequential-write mode)
 * @is_images_subnode: true to add hash even if key name hint is provided
 *
 * If do_add_hash is false (default) and there is a key name hint, try to add
 * a sign node to parent. Otherwise, just add a CRC. Rationale: if conf have
 * to be signed, image/dt have to be hashed even if there is a key name hint.
 */
static void fit_add_hash_or_sign(struct imgtool *itl, void *fdt,
				 bool is_images_subnode)
{
	const char *hash_algo = "crc32";
	bool do_hash = false;
	bool do_sign = false;

	switch (itl->auto_fit) {
	case AF_OFF:
		break;
	case AF_HASHED_IMG:
		do_hash = is_images_subnode;
		break;
	case AF_SIGNED_IMG:
		do_sign = is_images_subnode;
		break;
	case AF_SIGNED_CONF:
		if (is_images_subnode) {
			do_hash = true;
			hash_algo = "sha1";
		} else {
			do_sign = true;
		}
		break;
	default:
		fprintf(stderr,
			"%s: Unsupported auto FIT mode %u\n",
			itl->cmdname, itl->auto_fit);
		break;
	}

	if (do_hash) {
		fdt_begin_node(fdt, FIT_HASH_NODENAME);
		fdt_property_string(fdt, FIT_ALGO_PROP, hash_algo);
		fdt_end_node(fdt);
	}

	if (do_sign) {
		fdt_begin_node(fdt, FIT_SIG_NODENAME);
		fdt_property_string(fdt, FIT_ALGO_PROP, itl->algo_name);
		fdt_property_string(fdt, FIT_KEY_HINT, itl->keyname);
		fdt_end_node(fdt);
	}
}

/**
 * fit_write_images() - Write out a list of images to the FIT
 *
 * We always include the main image (itl->datafile). If there are device
 * tree files, we include an fdt- node for each of those too.
 */
static int fit_write_images(struct imgtool *itl, char *fdt)
{
	struct content_info *cont;
	const char *typename;
	char str[100];
	int upto;
	int ret;

	fdt_begin_node(fdt, "images");

	/* First the main image */
	typename = genimg_get_type_short_name(itl->fit_image_type);
	snprintf(str, sizeof(str), "%s-1", typename);
	fdt_begin_node(fdt, str);
	fdt_property_string(fdt, FIT_DESC_PROP, itl->imagename);
	fdt_property_string(fdt, FIT_TYPE_PROP, typename);
	fdt_property_string(fdt, FIT_ARCH_PROP,
			    genimg_get_arch_short_name(itl->arch));
	fdt_property_string(fdt, FIT_OS_PROP,
			    genimg_get_os_short_name(itl->os));
	fdt_property_string(fdt, FIT_COMP_PROP,
			    genimg_get_comp_short_name(itl->comp));
	fdt_property_u32(fdt, FIT_LOAD_PROP, itl->addr);
	fdt_property_u32(fdt, FIT_ENTRY_PROP, itl->ep);

	/*
	 * Put data last since it is large. SPL may only load the first part
	 * of the DT, so this way it can access all the above fields.
	 */
	ret = fdt_property_file(itl, fdt, FIT_DATA_PROP, itl->datafile);
	if (ret)
		return ret;
	fit_add_hash_or_sign(itl, fdt, true);
	fdt_end_node(fdt);

	/* Now the device tree files if available */
	upto = 0;
	for (cont = itl->content_head; cont; cont = cont->next) {
		if (cont->type != IH_TYPE_FLATDT)
			continue;
		typename = genimg_get_type_short_name(cont->type);
		snprintf(str, sizeof(str), "%s-%d", FIT_FDT_PROP, ++upto);
		fdt_begin_node(fdt, str);

		get_basename(str, sizeof(str), cont->fname);
		fdt_property_string(fdt, FIT_DESC_PROP, str);
		ret = fdt_property_file(itl, fdt, FIT_DATA_PROP,
					cont->fname);
		if (ret)
			return ret;
		fdt_property_string(fdt, FIT_TYPE_PROP, typename);
		fdt_property_string(fdt, FIT_ARCH_PROP,
				    genimg_get_arch_short_name(itl->arch));
		fdt_property_string(fdt, FIT_COMP_PROP,
				    genimg_get_comp_short_name(IH_COMP_NONE));
		fit_add_hash_or_sign(itl, fdt, true);
		if (ret)
			return ret;
		fdt_end_node(fdt);
	}

	/* And a ramdisk file if available */
	if (itl->fit_ramdisk) {
		fdt_begin_node(fdt, FIT_RAMDISK_PROP "-1");

		fdt_property_string(fdt, FIT_TYPE_PROP, FIT_RAMDISK_PROP);
		fdt_property_string(fdt, FIT_OS_PROP,
				    genimg_get_os_short_name(itl->os));
		fdt_property_string(fdt, FIT_ARCH_PROP,
				    genimg_get_arch_short_name(itl->arch));

		ret = fdt_property_file(itl, fdt, FIT_DATA_PROP,
					itl->fit_ramdisk);
		if (ret)
			return ret;
		fit_add_hash_or_sign(itl, fdt, true);
		if (ret)
			return ret;
		fdt_end_node(fdt);
	}

	fdt_end_node(fdt);

	return 0;
}

/**
 * fit_write_configs() - Write out a list of configurations to the FIT
 *
 * If there are device tree files, we include a configuration for each, which
 * selects the main image (params->datafile) and its corresponding device
 * tree file.
 *
 * Otherwise we just create a configuration with the main image in it.
 */
static void fit_write_configs(struct imgtool *itl, char *fdt)
{
	struct content_info *cont;
	const char *typename;
	char str[100];
	int upto;

	fdt_begin_node(fdt, "configurations");
	fdt_property_string(fdt, FIT_DEFAULT_PROP, "conf-1");

	upto = 0;
	for (cont = itl->content_head; cont; cont = cont->next) {
		if (cont->type != IH_TYPE_FLATDT)
			continue;
		typename = genimg_get_type_short_name(cont->type);
		snprintf(str, sizeof(str), "conf-%d", ++upto);
		fdt_begin_node(fdt, str);

		get_basename(str, sizeof(str), cont->fname);
		fdt_property_string(fdt, FIT_DESC_PROP, str);

		typename = genimg_get_type_short_name(itl->fit_image_type);
		snprintf(str, sizeof(str), "%s-1", typename);
		fdt_property_string(fdt, typename, str);

		if (itl->fit_ramdisk)
			fdt_property_string(fdt, FIT_RAMDISK_PROP,
					    FIT_RAMDISK_PROP "-1");

		snprintf(str, sizeof(str), FIT_FDT_PROP "-%d", upto);
		fdt_property_string(fdt, FIT_FDT_PROP, str);
		fit_add_hash_or_sign(itl, fdt, false);
		fdt_end_node(fdt);
	}

	if (!upto) {
		fdt_begin_node(fdt, "conf-1");
		typename = genimg_get_type_short_name(itl->fit_image_type);
		snprintf(str, sizeof(str), "%s-1", typename);
		fdt_property_string(fdt, typename, str);

		if (itl->fit_ramdisk)
			fdt_property_string(fdt, FIT_RAMDISK_PROP,
					    FIT_RAMDISK_PROP "-1");
		fit_add_hash_or_sign(itl, fdt, false);

		fdt_end_node(fdt);
	}

	fdt_end_node(fdt);
}

static int fit_build_fdt(struct imgtool *itl, char *fdt, int size)
{
	int ret;

	ret = fdt_create(fdt, size);
	if (ret)
		return ret;
	fdt_finish_reservemap(fdt);
	fdt_begin_node(fdt, "");
	fdt_property_strf(fdt, FIT_DESC_PROP,
			  "%s image with one or more FDT blobs",
			  genimg_get_type_name(itl->fit_image_type));
	fdt_property_strf(fdt, "creator", "U-Boot mkimage %s", PLAIN_VERSION);
	fdt_property_u32(fdt, "#address-cells", 1);
	ret = fit_write_images(itl, fdt);
	if (ret)
		return ret;
	fit_write_configs(itl, fdt);
	fdt_end_node(fdt);
	ret = fdt_finish(fdt);
	if (ret)
		return ret;

	return fdt_totalsize(fdt);
}

static int fit_build(struct imgtool *itl, const char *fname)
{
	char *buf;
	int size;
	int ret;
	int fd;

	size = fit_calc_size(itl);
	if (size < 0)
		return -1;
	buf = calloc(1, size);
	if (!buf) {
		fprintf(stderr, "%s: Out of memory (%d bytes)\n",
			itl->cmdname, size);
		return -1;
	}
	ret = fit_build_fdt(itl, buf, size);
	if (ret < 0) {
		fprintf(stderr, "%s: Failed to build FIT image\n",
			itl->cmdname);
		goto err_buf;
	}
	size = ret;
	fd = open(fname, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
	if (fd < 0) {
		fprintf(stderr, "%s: Can't open %s: %s\n",
			itl->cmdname, fname, strerror(errno));
		goto err_buf;
	}
	ret = write(fd, buf, size);
	if (ret != size) {
		fprintf(stderr, "%s: Can't write %s: %s\n",
			itl->cmdname, fname, strerror(errno));
		goto err;
	}
	close(fd);
	free(buf);

	return 0;
err:
	close(fd);
err_buf:
	free(buf);
	return -1;
}

/**
 * fit_extract_data() - Move all data outside the FIT
 *
 * This takes a normal FIT file and removes all the 'data' properties from it.
 * The data is placed in an area after the FIT so that it can be accessed
 * using an offset into that area. The 'data' properties turn into
 * 'data-offset' properties.
 *
 * This function cannot cope with FITs with 'data-offset' properties. All
 * data must be in 'data' properties on entry.
 */
static int fit_extract_data(struct imgtool *itl, const char *fname)
{
	void *buf = NULL;
	int buf_ptr;
	int fit_size, unpadded_size, new_size, pad_boundary;
	int fd;
	struct stat sbuf;
	void *fdt;
	int ret;
	int images;
	int node;
	int image_number;
	int align_size;

	align_size = itl->bl_len ? itl->bl_len : 4;
	fd = mmap_fdt(itl->cmdname, fname, 0, &fdt, &sbuf, false, false);
	if (fd < 0)
		return -EIO;
	fit_size = fdt_totalsize(fdt);

	images = fdt_path_offset(fdt, FIT_IMAGES_PATH);
	if (images < 0) {
		debug("%s: Cannot find /images node: %d\n", __func__, images);
		ret = -EINVAL;
		goto err_munmap;
	}
	image_number = fdtdec_get_child_count(fdt, images);

	/*
	 * Allocate space to hold the image data we will extract,
	 * extral space allocate for image alignment to prevent overflow.
	 */
	buf = calloc(1, fit_size + (align_size * image_number));
	if (!buf) {
		ret = -ENOMEM;
		goto err_munmap;
	}
	buf_ptr = 0;

	for (node = fdt_first_subnode(fdt, images);
	     node >= 0;
	     node = fdt_next_subnode(fdt, node)) {
		const char *data;
		int len;

		data = fdt_getprop(fdt, node, FIT_DATA_PROP, &len);
		if (!data)
			continue;
		memcpy(buf + buf_ptr, data, len);
		debug("Extracting data size %x\n", len);

		ret = fdt_delprop(fdt, node, FIT_DATA_PROP);
		if (ret) {
			ret = -EPERM;
			goto err_munmap;
		}
		if (itl->external_offset > 0) {
			/* An external offset positions the data absolutely. */
			fdt_setprop_u32(fdt, node, FIT_DATA_POSITION_PROP,
					itl->external_offset + buf_ptr);
		} else {
			fdt_setprop_u32(fdt, node, FIT_DATA_OFFSET_PROP,
					buf_ptr);
		}
		fdt_setprop_u32(fdt, node, FIT_DATA_SIZE_PROP, len);
		buf_ptr += ALIGN(len, align_size);
	}

	/* Pack the FDT and place the data after it */
	fdt_pack(fdt);

	unpadded_size = fdt_totalsize(fdt);
	new_size = ALIGN(unpadded_size, align_size);
	fdt_set_totalsize(fdt, new_size);
	if (unpadded_size < fit_size) {
		pad_boundary = new_size < fit_size ? new_size : fit_size;
		memset(fdt + unpadded_size, 0, pad_boundary - unpadded_size);
	}
	debug("Size reduced from %x to %x\n", fit_size, fdt_totalsize(fdt));
	debug("External data size %x\n", buf_ptr);
	munmap(fdt, sbuf.st_size);

	if (ftruncate(fd, new_size)) {
		debug("%s: Failed to truncate file: %s\n", __func__,
		      strerror(errno));
		ret = -EIO;
		goto err;
	}

	/* Check if an offset for the external data was set. */
	if (itl->external_offset > 0) {
		if (itl->external_offset < new_size) {
			fprintf(stderr,
				"External offset %x overlaps FIT length %x\n",
				itl->external_offset, new_size);
			ret = -EINVAL;
			goto err;
		}
		new_size = itl->external_offset;
	}
	if (lseek(fd, new_size, SEEK_SET) < 0) {
		debug("%s: Failed to seek to end of file: %s\n", __func__,
		      strerror(errno));
		ret = -EIO;
		goto err;
	}
	if (write(fd, buf, buf_ptr) != buf_ptr) {
		debug("%s: Failed to write external data to file %s\n",
		      __func__, strerror(errno));
		ret = -EIO;
		goto err;
	}
	free(buf);
	close(fd);
	return 0;

err_munmap:
	munmap(fdt, sbuf.st_size);
err:
	free(buf);
	close(fd);
	return ret;
}

static int fit_import_data(struct imgtool *itl, const char *fname)
{
	void *fdt, *old_fdt;
	void *data = NULL;
	const char *ext_data_prop = NULL;
	int fit_size, new_size, size, data_base;
	int fd;
	struct stat sbuf;
	int ret;
	int images;
	int node;

	fd = mmap_fdt(itl->cmdname, fname, 0, &old_fdt, &sbuf, false, false);
	if (fd < 0)
		return -EIO;
	fit_size = fdt_totalsize(old_fdt);
	data_base = ALIGN(fit_size, 4);

	/* Allocate space to hold the new FIT */
	size = sbuf.st_size + 16384;
	fdt = calloc(1, size);
	if (!fdt) {
		fprintf(stderr, "%s: Failed to allocate memory (%d bytes)\n",
			__func__, size);
		ret = -ENOMEM;
		goto err_munmap;
	}
	ret = fdt_open_into(old_fdt, fdt, size);
	if (ret) {
		debug("%s: Failed to expand FIT: %s\n", __func__,
		      fdt_strerror(errno));
		ret = -EINVAL;
		goto err_munmap;
	}

	images = fdt_path_offset(fdt, FIT_IMAGES_PATH);
	if (images < 0) {
		debug("%s: Cannot find /images node: %d\n", __func__, images);
		ret = -EINVAL;
		goto err_munmap;
	}

	for (node = fdt_first_subnode(fdt, images);
	     node >= 0;
	     node = fdt_next_subnode(fdt, node)) {
		int buf_ptr;
		int len;

		/*
		 * FIT_DATA_OFFSET_PROP and FIT_DATA_POSITION_PROP are never both present,
		 *  but if they are, prefer FIT_DATA_OFFSET_PROP as it was there first
		 */
		buf_ptr = fdtdec_get_int(fdt, node, FIT_DATA_POSITION_PROP, -1);
		if (buf_ptr != -1) {
			ext_data_prop = FIT_DATA_POSITION_PROP;
			data = old_fdt + buf_ptr;
		}
		buf_ptr = fdtdec_get_int(fdt, node, FIT_DATA_OFFSET_PROP, -1);
		if (buf_ptr != -1) {
			ext_data_prop = FIT_DATA_OFFSET_PROP;
			data = old_fdt + data_base + buf_ptr;
		}
		len = fdtdec_get_int(fdt, node, FIT_DATA_SIZE_PROP, -1);
		if (!data || len == -1)
			continue;
		debug("Importing data size %x\n", len);

		ret = fdt_setprop(fdt, node, FIT_DATA_PROP, data, len);
		ret = fdt_delprop(fdt, node, ext_data_prop);

		if (ret) {
			debug("%s: Failed to write property: %s\n", __func__,
			      fdt_strerror(ret));
			ret = -EINVAL;
			goto err_munmap;
		}
	}

	munmap(old_fdt, sbuf.st_size);

	/* Close the old fd so we can re-use it. */
	close(fd);

	/* Pack the FDT and place the data after it */
	fdt_pack(fdt);

	new_size = fdt_totalsize(fdt);
	debug("Size expanded from %x to %x\n", fit_size, new_size);

	fd = open(fname, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
	if (fd < 0) {
		fprintf(stderr, "%s: Can't open %s: %s\n",
			itl->cmdname, fname, strerror(errno));
		ret = -EIO;
		goto err;
	}
	if (write(fd, fdt, new_size) != new_size) {
		debug("%s: Failed to write external data to file %s\n",
		      __func__, strerror(errno));
		ret = -EIO;
		goto err;
	}

	free(fdt);
	close(fd);
	return 0;

err_munmap:
	munmap(old_fdt, sbuf.st_size);
err:
	free(fdt);
	close(fd);
	return ret;
}

/**
 * fit_handle_file - main FIT file processing function
 *
 * fit_handle_file() runs dtc to convert .its to .itb, includes
 * binary data, updates timestamp property and calculates hashes.
 *
 * datafile  - .its file
 * imagefile - .itb file
 *
 * returns:
 *     only on success, otherwise calls exit (EXIT_FAILURE);
 */
static int fit_handle_file(struct imgtool *itl)
{
	char tmpfile[MKIMAGE_MAX_TMPFILE_LEN];
	char bakfile[MKIMAGE_MAX_TMPFILE_LEN + 4] = {0};
	char cmd[MKIMAGE_MAX_DTC_CMDLINE_LEN];
	size_t size_inc;
	int ret;

	/* Flattened Image Tree (FIT) format  handling */
	debug ("FIT format handling\n");

	/* call dtc to include binary properties into the tmp file */
	if (strlen(itl->imagefile) +
		strlen (MKIMAGE_TMPFILE_SUFFIX) + 1 > sizeof (tmpfile)) {
		fprintf (stderr, "%s: Image file name (%s) too long, "
				"can't create tmpfile.\n",
				itl->imagefile, itl->cmdname);
		return (EXIT_FAILURE);
	}
	sprintf(tmpfile, "%s%s", itl->imagefile, MKIMAGE_TMPFILE_SUFFIX);

	/* We either compile the source file, or use the existing FIT image */
	if (itl->auto_fit) {
		if (fit_build(itl, tmpfile)) {
			fprintf(stderr, "%s: failed to build FIT\n",
				itl->cmdname);
			return EXIT_FAILURE;
		}
		*cmd = '\0';
	} else if (itl->datafile) {
		/* dtc -I dts -O dtb -p 500 -o tmpfile datafile */
		snprintf(cmd, sizeof(cmd), "%s %s -o \"%s\" \"%s\"",
			 MKIMAGE_DTC, itl->dtc, tmpfile, itl->datafile);
		debug("Trying to execute \"%s\"\n", cmd);
	} else {
		snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"",
			 itl->imagefile, tmpfile);
	}
	if (strlen(cmd) >= MKIMAGE_MAX_DTC_CMDLINE_LEN - 1) {
		fprintf(stderr, "WARNING: command-line for FIT creation might be truncated and will probably fail.\n");
	}

	if (*cmd && system(cmd) == -1) {
		fprintf (stderr, "%s: system(%s) failed: %s\n",
				itl->cmdname, cmd, strerror(errno));
		goto err_system;
	}

	/* Move the data so it is internal to the FIT, if needed */
	ret = fit_import_data(itl, tmpfile);
	if (ret)
		goto err_system;

	/*
	 * Copy the tmpfile to bakfile, then in the following loop
	 * we copy bakfile to tmpfile. So we always start from the
	 * beginning.
	 */
	sprintf(bakfile, "%s%s", tmpfile, ".bak");
	rename(tmpfile, bakfile);

	/*
	 * Set hashes for images in the blob. Unfortunately we may need more
	 * space in either FDT, so keep trying until we succeed.
	 *
	 * Note: this is pretty inefficient for signing, since we must
	 * calculate the signature every time. It would be better to calculate
	 * all the data and then store it in a separate step. However, this
	 * would be considerably more complex to implement. Generally a few
	 * steps of this loop is enough to sign with several keys.
	 */
	for (size_inc = 0; size_inc < 64 * 1024; size_inc += 1024) {
		if (copyfile(bakfile, tmpfile) < 0) {
			printf("Can't copy %s to %s\n", bakfile, tmpfile);
			ret = -EIO;
			break;
		}
		ret = fit_add_file_data(itl, size_inc, tmpfile);
		if (!ret || ret != -ENOSPC)
			break;
	}

	if (ret) {
		fprintf(stderr, "%s Can't add hashes to FIT blob: %d\n",
			itl->cmdname, ret);
		goto err_system;
	}

	/* Move the data so it is external to the FIT, if requested */
	if (itl->external_data) {
		ret = fit_extract_data(itl, tmpfile);
		if (ret)
			goto err_system;
	}

	if (rename(tmpfile, itl->imagefile) == -1) {
		fprintf (stderr, "%s: Can't rename %s to %s: %s\n",
				itl->cmdname, tmpfile, itl->imagefile,
				strerror (errno));
		unlink (tmpfile);
		unlink(bakfile);
		unlink(itl->imagefile);
		return EXIT_FAILURE;
	}
	unlink(bakfile);
	return EXIT_SUCCESS;

err_system:
	unlink(tmpfile);
	unlink(bakfile);
	return -1;
}

/**
 * fit_image_extract - extract a FIT component image
 * @fit: pointer to the FIT format image header
 * @image_noffset: offset of the component image node
 * @file_name: name of the file to store the FIT sub-image
 *
 * returns:
 *     zero in case of success or a negative value if fail.
 */
static int fit_image_extract(
	const void *fit,
	int image_noffset,
	const char *file_name)
{
	const void *file_data;
	size_t file_size = 0;
	int ret;

	/* get the data address and size of component at offset "image_noffset" */
	ret = fit_image_get_data(fit, image_noffset, &file_data, &file_size);
	if (ret) {
		fprintf(stderr, "Could not get component information\n");
		return ret;
	}

	/* save the "file_data" into the file specified by "file_name" */
	return imagetool_save_subimage(file_name, (ulong) file_data, file_size);
}

/**
 * fit_extract_contents - retrieve a sub-image component from the FIT image
 * @ptr: pointer to the FIT format image header
 * @itl: command line parameters
 *
 * returns:
 *     zero in case of success or a negative value if fail.
 */
static int fit_extract_contents(void *ptr, struct imgtool *itl)
{
	int images_noffset;
	int noffset;
	int ndepth;
	const void *fit = ptr;
	int count = 0;
	const char *p;

	/* Indent string is defined in header image.h */
	p = IMAGE_INDENT_STRING;

	/* Find images parent node offset */
	images_noffset = fdt_path_offset(fit, FIT_IMAGES_PATH);
	if (images_noffset < 0) {
		printf("Can't find images parent node '%s' (%s)\n",
		       FIT_IMAGES_PATH, fdt_strerror(images_noffset));
		return -1;
	}

	/* Avoid any overrun */
	count = fit_get_subimage_count(fit, images_noffset);
	if (itl->pflag < 0 || count <= itl->pflag) {
		printf("No such component at '%d'\n", itl->pflag);
		return -1;
	}

	/* Process its subnodes, extract the desired component from image */
	for (ndepth = 0, count = 0,
		noffset = fdt_next_node(fit, images_noffset, &ndepth);
		(noffset >= 0) && (ndepth > 0);
		noffset = fdt_next_node(fit, noffset, &ndepth)) {
		if (ndepth == 1) {
			/*
			 * Direct child node of the images parent node,
			 * i.e. component image node.
			 */
			if (itl->pflag == count) {
				printf("Extracted:\n%s Image %u (%s)\n", p,
				       count, fit_get_name(fit, noffset, NULL));

				fit_image_print(fit, noffset, p);

				return fit_image_extract(fit, noffset,
						itl->outfile);
			}

			count++;
		}
	}

	return 0;
}

static int fit_check_params(struct imgtool *itl)
{
	if (itl->auto_fit)
		return 0;
	return	((itl->dflag && itl->fflag) ||
		 (itl->fflag && itl->lflag) ||
		 (itl->lflag && itl->dflag));
}

U_BOOT_IMAGE_TYPE(
	fitimage,
	"FIT Image support",
	sizeof(struct legacy_img_hdr),
	(void *)&header,
	fit_check_params,
	fit_verify_header,
	fit_print_header,
	NULL,
	fit_extract_contents,
	fit_check_image_types,
	fit_handle_file,
	NULL /* FIT images use DTB header */
);
