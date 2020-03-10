// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Mostly taken from coreboot
 */

#include <common.h>
#include <dm.h>
#include <tables_csum.h>
#include <dm/acpi.h>
#include <acpi_device.h>
#include <acpi_table.h>

int acpi_write_hpet(struct acpi_ctx *ctx, const struct udevice *dev)
{
	struct acpi_hpet *hpet;
	int ret;

	/*
	 * We explicitly add these tables later on:
	 */
	log_debug("ACPI:    * HPET\n");

	hpet = ctx->current;
	acpi_inc_align(ctx, sizeof(struct acpi_hpet));
	acpi_create_hpet(hpet);
	ret = acpi_add_table(ctx, hpet);
	if (ret)
		return log_msg_ret("add", ret);

	return 0;
}

/* http://www.intel.com/hardwaredesign/hpetspec_1.pdf */
int acpi_create_hpet(struct acpi_hpet *hpet)
{
	struct acpi_table_header *header = &hpet->header;
	struct acpi_gen_regaddr *addr = &hpet->addr;

	memset((void *)hpet, 0, sizeof(struct acpi_hpet));
	if (!header)
		return -EFAULT;

	/* Fill out header fields. */
	acpi_fill_header(header, "HPET");

	header->aslc_revision = ASL_REVISION;
	header->length = sizeof(struct acpi_hpet);
	header->revision = acpi_get_table_revision(ACPITAB_HPET);

	/* Fill out HPET address. */
	addr->space_id = 0; /* Memory */
	addr->bit_width = 64;
	addr->bit_offset = 0;
	addr->addrl = CONFIG_HPET_ADDRESS & 0xffffffff;
	addr->addrh = ((unsigned long long)CONFIG_HPET_ADDRESS) >> 32;

	hpet->id = *(unsigned int *)CONFIG_HPET_ADDRESS;
	hpet->number = 0;
	hpet->min_tick = 0; /* HPET_MIN_TICKS */

	header->checksum = table_compute_checksum((void *)hpet,
						  sizeof(struct acpi_hpet));

	return 0;
}

void acpi_create_dbg2(struct acpi_dbg2_header *dbg2,
		      int port_type, int port_subtype,
		      struct acpi_gen_regaddr *address, u32 address_size,
		      const char *device_path)
{
	uintptr_t current;
	struct acpi_dbg2_device *device;
	u32 *dbg2_addr_size;
	struct acpi_table_header *header;
	size_t path_len;
	const char *path;
	char *namespace;

	/* Fill out header fields. */
	current = (uintptr_t)dbg2;
	memset(dbg2, '\0', sizeof(struct acpi_dbg2_header));
	header = &dbg2->header;

	header->revision = acpi_get_table_revision(ACPITAB_DBG2);
	acpi_fill_header(header, "DBG2");
	header->aslc_revision = ASL_REVISION;

	/* One debug device defined */
	dbg2->devices_offset = sizeof(struct acpi_dbg2_header);
	dbg2->devices_count = 1;
	current += sizeof(struct acpi_dbg2_header);

	/* Device comes after the header */
	device = (struct acpi_dbg2_device *)current;
	memset(device, 0, sizeof(struct acpi_dbg2_device));
	current += sizeof(struct acpi_dbg2_device);

	device->revision = 0;
	device->address_count = 1;
	device->port_type = port_type;
	device->port_subtype = port_subtype;

	/* Base Address comes after device structure */
	memcpy((void *)current, address, sizeof(struct acpi_gen_regaddr));
	device->base_address_offset = current - (uintptr_t)device;
	current += sizeof(struct acpi_gen_regaddr);

	/* Address Size comes after address structure */
	dbg2_addr_size = (uint32_t *)current;
	device->address_size_offset = current - (uintptr_t)device;
	*dbg2_addr_size = address_size;
	current += sizeof(uint32_t);

	/* Namespace string comes last, use '.' if not provided */
	path = device_path ? : ".";
	/* Namespace string length includes NULL terminator */
	path_len = strlen(path) + 1;
	namespace = (char *)current;
	device->namespace_string_length = path_len;
	device->namespace_string_offset = current - (uintptr_t)device;
	strncpy(namespace, path, path_len);
	current += path_len;

	/* Update structure lengths and checksum */
	device->length = current - (uintptr_t)device;
	header->length = current - (uintptr_t)dbg2;
	header->checksum = table_compute_checksum((uint8_t *)dbg2,
						  header->length);
}

int acpi_write_dbg2_pci_uart(struct acpi_ctx *ctx, struct udevice *dev,
			     uint access_size)
{
	struct acpi_dbg2_header *dbg2 = ctx->current;
	char path[ACPI_PATH_MAX];
	struct acpi_gen_regaddr address;
	phys_addr_t addr;
	int ret;

	if (!dev) {
		log_err("Device not found\n");
		return -ENODEV;
	}
	if (!device_active(dev)) {
		log_info("Device not enabled\n");
		return -EACCES;
	}
	/*
	 * PCI devices don't remember their resource allocation information in
	 * U-Boot at present. We assume that MMIO is used for the UART and that
	 * the address space is 32 bytes: ns16550 uses 8 registers of up to
	 * 32-bits each. This is only for debugging so it is not a big deal.
	 */
	addr = dm_pci_read_bar32(dev, 0);
	printf("UART addr %lx\n", (ulong)addr);

	memset(&address, '\0', sizeof(address));
	address.space_id = ACPI_ADDRESS_SPACE_MEMORY;
	address.addrl = (uint32_t)addr;
	address.addrh = (uint32_t)((addr >> 32) & 0xffffffff);
	address.access_size = access_size;

	ret = acpi_device_path(dev, path, sizeof(path));
	if (ret)
		return log_msg_ret("path", ret);
	acpi_create_dbg2(dbg2, ACPI_DBG2_SERIAL_PORT,
			 ACPI_DBG2_16550_COMPATIBLE, &address, 0x1000, path);

	acpi_inc_align(ctx, dbg2->header.length);
	acpi_add_table(ctx, dbg2);

	return 0;
}

void acpi_fadt_common(struct acpi_fadt *fadt, struct acpi_facs *facs,
		      void *dsdt)
{
	struct acpi_table_header *header = &fadt->header;

	memset((void *)fadt, '\0', sizeof(struct acpi_fadt));

	acpi_fill_header(header, "FACP");
	header->length = sizeof(struct acpi_fadt);
	header->revision = 4;
	memcpy(header->oem_id, OEM_ID, 6);
	memcpy(header->oem_table_id, OEM_TABLE_ID, 8);
	memcpy(header->aslc_id, ASLC_ID, 4);
	header->aslc_revision = 1;

	fadt->firmware_ctrl = (unsigned long)facs;
	fadt->dsdt = (unsigned long)dsdt;

	fadt->x_firmware_ctl_l = (unsigned long)facs;
	fadt->x_firmware_ctl_h = 0;
	fadt->x_dsdt_l = (unsigned long)dsdt;
	fadt->x_dsdt_h = 0;

	fadt->preferred_pm_profile = ACPI_PM_MOBILE;

	/* Use ACPI 3.0 revision */
	fadt->header.revision = 4;
}

int acpi_create_dmar_drhd(struct acpi_ctx *ctx, uint flags, uint segment,
			  u64 bar)
{
	struct dmar_entry *drhd = ctx->current;

	memset(drhd, 0, sizeof(*drhd));
	drhd->type = DMAR_DRHD;
	drhd->length = sizeof(*drhd); /* will be fixed up later */
	drhd->flags = flags;
	drhd->segment = segment;
	drhd->bar = bar;
	acpi_inc(ctx, drhd->length);

	return 0;
}

int acpi_create_dmar_rmrr(struct acpi_ctx *ctx, uint segment, u64 bar,
			  u64 limit)
{
	struct dmar_rmrr_entry *rmrr = ctx->current;

	memset(rmrr, 0, sizeof(*rmrr));
	rmrr->type = DMAR_RMRR;
	rmrr->length = sizeof(*rmrr); /* will be fixed up later */
	rmrr->segment = segment;
	rmrr->bar = bar;
	rmrr->limit = limit;
	acpi_inc(ctx, rmrr->length);

	return 0;
}

void acpi_dmar_drhd_fixup(struct acpi_ctx *ctx, void *base)
{
	struct dmar_entry *drhd = base;

	drhd->length = ctx->current - base;
}

void acpi_dmar_rmrr_fixup(struct acpi_ctx *ctx, void *base)
{
	struct dmar_rmrr_entry *rmrr = base;

	rmrr->length = ctx->current - base;
}

static int acpi_create_dmar_ds(struct acpi_ctx *ctx, enum dev_scope_type type,
			       uint enumeration_id, pci_dev_t bdf)
{
	/* we don't support longer paths yet */
	const size_t dev_scope_length = sizeof(struct dev_scope) + 2;
	struct dev_scope *ds = ctx->current;

	memset(ds, 0, dev_scope_length);
	ds->type	= type;
	ds->length	= dev_scope_length;
	ds->enumeration	= enumeration_id;
	ds->start_bus	= PCI_BUS(bdf);
	ds->path[0].dev	= PCI_DEV(bdf);
	ds->path[0].fn	= PCI_FUNC(bdf);

	return ds->length;
}

int acpi_create_dmar_ds_pci_br(struct acpi_ctx *ctx, pci_dev_t bdf)
{
	return acpi_create_dmar_ds(ctx, SCOPE_PCI_SUB, 0, bdf);
}

int acpi_create_dmar_ds_pci(struct acpi_ctx *ctx, pci_dev_t bdf)
{
	return acpi_create_dmar_ds(ctx, SCOPE_PCI_ENDPOINT, 0, bdf);
}

int acpi_create_dmar_ds_ioapic(struct acpi_ctx *ctx, uint enumeration_id,
			       pci_dev_t bdf)
{
	return acpi_create_dmar_ds(ctx, SCOPE_IOAPIC, enumeration_id, bdf);
}

int acpi_create_dmar_ds_msi_hpet(struct acpi_ctx *ctx, uint enumeration_id,
				 pci_dev_t bdf)
{
	return acpi_create_dmar_ds(ctx, SCOPE_MSI_HPET, enumeration_id, bdf);
}
