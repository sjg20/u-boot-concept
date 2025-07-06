// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2015 Miao Yan <yanmiaobest@gmail.com>
 * (C) Copyright 2021 Asherah Connor <ashe@kivikakk.ee>
 */

#include <abuf.h>
#include <dm.h>
#include <env.h>
#include <mapmem.h>
#include <qfw.h>
#include <stdlib.h>
#include <dm/uclass.h>

int qfw_get_dev(struct udevice **devp)
{
	return uclass_first_device_err(UCLASS_QFW, devp);
}

int qfw_online_cpus(struct udevice *dev)
{
	u16 nb_cpus;

	qfw_read_entry(dev, FW_CFG_NB_CPUS, 2, &nb_cpus);

	return le16_to_cpu(nb_cpus);
}

int qfw_read_firmware_list(struct udevice *dev)
{
	int i;
	u32 count;
	struct fw_file *file;
	struct list_head *entry;

	struct qfw_dev *qdev = dev_get_uclass_priv(dev);

	/* don't read it twice */
	if (!list_empty(&qdev->fw_list))
		return 0;

	qfw_read_entry(dev, FW_CFG_FILE_DIR, 4, &count);
	if (!count)
		return 0;

	count = be32_to_cpu(count);
	for (i = 0; i < count; i++) {
		file = malloc(sizeof(*file));
		if (!file) {
			printf("error: allocating resource\n");
			goto err;
		}
		qfw_read_entry(dev, FW_CFG_INVALID,
			       sizeof(struct fw_cfg_file), &file->cfg);
		file->addr = 0;
		list_add_tail(&file->list, &qdev->fw_list);
	}

	return 0;

err:
	list_for_each(entry, &qdev->fw_list) {
		file = list_entry(entry, struct fw_file, list);
		free(file);
	}

	return -ENOMEM;
}

struct fw_file *qfw_find_file(struct udevice *dev, const char *name)
{
	struct list_head *entry;
	struct fw_file *file;

	struct qfw_dev *qdev = dev_get_uclass_priv(dev);

	list_for_each(entry, &qdev->fw_list) {
		file = list_entry(entry, struct fw_file, list);
		if (!strcmp(file->cfg.name, name))
			return file;
	}

	return NULL;
}

struct fw_file *qfw_file_iter_init(struct udevice *dev,
				   struct fw_cfg_file_iter *iter)
{
	struct qfw_dev *qdev = dev_get_uclass_priv(dev);

	iter->entry = qdev->fw_list.next;
	iter->end = &qdev->fw_list;
	return list_entry((struct list_head *)iter->entry,
			  struct fw_file, list);
}

struct fw_file *qfw_file_iter_next(struct fw_cfg_file_iter *iter)
{
	iter->entry = ((struct list_head *)iter->entry)->next;
	return list_entry((struct list_head *)iter->entry,
			  struct fw_file, list);
}

bool qfw_file_iter_end(struct fw_cfg_file_iter *iter)
{
	return iter->entry == iter->end;
}

/**
 * qfw_read_size() - Read the size of an entry
 *
 * @sel: Selector value, e.g. FW_CFG_SETUP_SIZE, FW_CFG_CMDLINE_SIZE
 * Return: Size of the entry
 */
static ulong qfw_read_size(struct udevice *qfw_dev, enum fw_cfg_selector sel)
{
	u32 size = 0;

	qfw_read_entry(qfw_dev, sel, 4, &size);

	return le32_to_cpu(size);
}

int qemu_fwcfg_read_info(struct udevice *qfw_dev, ulong *setupp, ulong *kernp,
			 ulong *initrdp, struct abuf *cmdline,
			 ulong *setup_addrp)
{
	uint cmdline_size;

	*setupp = qfw_read_size(qfw_dev, FW_CFG_SETUP_SIZE);
	*kernp = qfw_read_size(qfw_dev, FW_CFG_KERNEL_SIZE);
	*initrdp = qfw_read_size(qfw_dev, FW_CFG_INITRD_SIZE);
	cmdline_size = qfw_read_size(qfw_dev, FW_CFG_CMDLINE_SIZE);
	if (!*kernp)
		return -ENOENT;

	*setup_addrp = qfw_read_size(qfw_dev, FW_CFG_SETUP_ADDR);

	if (!abuf_init_size(cmdline, cmdline_size))
		return log_msg_ret("qri", -ENOMEM);
	qfw_read_entry(qfw_dev, FW_CFG_CMDLINE_DATA, cmdline_size,
		       cmdline->data);

	return 0;
}

void qemu_fwcfg_read_files(struct udevice *qfw_dev, const struct abuf *setup,
			   const struct abuf *kern, const struct abuf *initrd)
{
	if (setup->size) {
		qfw_read_entry(qfw_dev, FW_CFG_SETUP_DATA, setup->size,
			       setup->data);
	}
	qfw_read_entry(qfw_dev, FW_CFG_KERNEL_DATA, kern->size, kern->data);
	if (initrd->size) {
		qfw_read_entry(qfw_dev, FW_CFG_INITRD_DATA, initrd->size,
			       initrd->data);
	}
}

int qemu_fwcfg_setup_kernel(struct udevice *qfw_dev, ulong load_addr,
			    ulong initrd_addr)
{
	ulong setup_size, kernel_size, initrd_size, setup_addr;
	struct abuf cmdline, setup, kern, initrd;
	int ret;

	ret = qemu_fwcfg_read_info(qfw_dev, &setup_size, &initrd_size,
				   &kernel_size, &cmdline, &setup_addr);
	if (ret) {
		printf("fatal: no kernel available\n");
		return log_msg_ret("qsk", ret);
	}

	/*
	 * always put the setup area where QEMU wants it, since it includes
	 * absolute pointers to itself
	 */
	abuf_init_const_addr(&setup, setup_addr, 0);
	abuf_init_const_addr(&kern, load_addr, 0);

	abuf_init_const_addr(&initrd, initrd_addr, 0);
	qemu_fwcfg_read_files(qfw_dev, &setup, &kern, &initrd);

	env_set_hex("filesize", kern.size);

	if (!initrd_size)
		printf("warning: no initrd available\n");
	else
		env_set_hex("filesize", initrd_size);

	if (cmdline.data) {
		/*
		 * if kernel cmdline only contains '\0', (e.g. no -append
		 * when invoking qemu), do not update bootargs
		 */
		if (*(char *)cmdline.data) {
			if (env_set("bootargs", cmdline.data) < 0)
				printf("warning: unable to change bootargs\n");
		}
	}
	abuf_uninit(&cmdline);

	printf("loading kernel to address %lx size %zx", abuf_addr(&kern),
	       kern.size);
	if (initrd_size)
		printf(" initrd %lx size %lx\n", abuf_addr(&initrd),
		       initrd_size);
	else
		printf("\n");

	return 0;
}

static int qfw_locate_file(struct udevice *dev, const char *fname,
			   enum fw_cfg_selector *selectp, ulong *sizep)
{
	struct fw_file *file;
	int ret;

	/* make sure fw_list is loaded */
	ret = qfw_read_firmware_list(dev);
	if (ret) {
		printf("error: can't read firmware file list\n");
		return -EINVAL;
	}

	file = qfw_find_file(dev, fname);
	if (!file) {
		printf("error: can't find %s\n", fname);
		return -ENOENT;
	}

	*selectp = be16_to_cpu(file->cfg.select);
	*sizep = be32_to_cpu(file->cfg.size);

	return 0;
}

int qfw_load_file(struct udevice *dev, const char *fname, ulong addr)
{
	enum fw_cfg_selector select;
	ulong size;
	int ret;

	ret = qfw_locate_file(dev, fname, &select, &size);
	if (ret)
		return ret;

	qfw_read_entry(dev, select, size, map_sysmem(addr, size));

	return 0;
}

int qfw_get_file(struct udevice *dev, const char *fname, struct abuf *loader)
{
	enum fw_cfg_selector select;
	ulong size;
	int ret;

	ret = qfw_locate_file(dev, fname, &select, &size);
	if (ret)
		return ret;

	if (!abuf_init_size(loader, size)) {
		printf("error: table-loader out of memory\n");
		return -ENOMEM;
	}

	qfw_read_entry(dev, select, size, loader->data);

	return 0;
}

int qfw_get_table_loader(struct udevice *dev, struct abuf *loader)
{
	int ret;

	ret = qfw_get_file(dev, "etc/table-loader", loader);
	if (ret)
		return ret;
	if ((loader->size % sizeof(struct bios_linker_entry)) != 0) {
		printf("error: table-loader maybe corrupted\n");
		abuf_uninit(loader);
		return -EINVAL;
	}

	return 0;
}
