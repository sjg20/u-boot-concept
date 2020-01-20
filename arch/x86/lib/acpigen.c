// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Google LLC
 * Mostly taken from coreboot
 */

#include <common.h>
#include <tables_csum.h>
#include <dm/acpi.h>
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
