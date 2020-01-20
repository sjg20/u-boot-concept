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
