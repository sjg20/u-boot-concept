// SPDX-License-Identifier: GPL-2.0
/*
 * Utility functions for ACPI
 *
 * Copyright 2023 Google LLC
 */

#include <errno.h>
#include <mapmem.h>
#include <tables_csum.h>
#include <version_string.h>
#include <acpi/acpi_table.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * OEM_REVISION is 32-bit unsigned number. It should be increased only when
 * changing software version. Therefore it should not depend on build time.
 * U-Boot calculates it from U-Boot version and represent it in hexadecimal
 * notation. As U-Boot version is in form year.month set low 8 bits to 0x01
 * to have valid date. So for U-Boot version 2021.04 OEM_REVISION is set to
 * value 0x20210401.
 */
#define OEM_REVISION ((((version_num / 1000) % 10) << 28) | \
		      (((version_num / 100) % 10) << 24) | \
		      (((version_num / 10) % 10) << 20) | \
		      ((version_num % 10) << 16) | \
		      (((version_num_patch / 10) % 10) << 12) | \
		      ((version_num_patch % 10) << 8) | \
		      0x01)

void acpi_update_checksum(struct acpi_table_header *header)
{
	header->checksum = 0;
	header->checksum = table_compute_checksum(header, header->length);
}

static int setup_search(bool *has_xsdtp, struct acpi_rsdp **rsdpp, void **ptrp)
{
	struct acpi_rsdp *rsdp;
	struct acpi_rsdt *rsdt;
	struct acpi_xsdt *xsdt;
	int len, count;

	rsdp = map_sysmem(gd_acpi_start(), 0);
	if (!rsdp)
		return 0;
	if (rsdp->xsdt_address) {
		*has_xsdtp = true;
		xsdt = nomap_sysmem(rsdp->xsdt_address, 0);
		len = xsdt->header.length - sizeof(xsdt->header);
		count = len / sizeof(u64);
		*ptrp = xsdt;
	} else {
		if (!rsdp->rsdt_address)
			return 0;
		*has_xsdtp = false;
		rsdt = nomap_sysmem(rsdp->rsdt_address, 0);
		len = rsdt->header.length - sizeof(rsdt->header);
		count = len / sizeof(u32);
		*ptrp = rsdt;
	}
	if (rsdpp)
		*rsdpp = rsdp;

	return count;
}

struct acpi_table_header *acpi_find_table(const char *sig)
{
	struct acpi_rsdt *rsdt;
	struct acpi_xsdt *xsdt;
	bool has_xsdt;
	int i, count;
	void *ptr;

	count = setup_search(&has_xsdt, NULL, &ptr);
	if (!count)
		return NULL;
	xsdt = ptr;
	rsdt = ptr;

	for (i = 0; i < count; i++) {
		struct acpi_table_header *hdr;

		if (has_xsdt)
			hdr = nomap_sysmem(xsdt->entry[i], 0);
		else
			hdr = nomap_sysmem(rsdt->entry[i], 0);
		if (!memcmp(hdr->signature, sig, ACPI_NAME_LEN))
			return hdr;
		if (!memcmp(hdr->signature, "FACP", ACPI_NAME_LEN)) {
			struct acpi_fadt *fadt = (struct acpi_fadt *)hdr;

			if (!memcmp(sig, "DSDT", ACPI_NAME_LEN)) {
				void *dsdt;

				if (fadt->header.revision >= 3 && fadt->x_dsdt)
					dsdt = nomap_sysmem(fadt->x_dsdt, 0);
				else if (fadt->dsdt)
					dsdt = nomap_sysmem(fadt->dsdt, 0);
				else
					dsdt = NULL;
				return dsdt;
			}

			if (!memcmp(sig, "FACS", ACPI_NAME_LEN)) {
				void *facs;

				if (fadt->header.revision >= 3 &&
				    fadt->x_firmware_ctrl)
					facs = nomap_sysmem(fadt->x_firmware_ctrl, 0);
				else if (fadt->firmware_ctrl)
					facs = nomap_sysmem(fadt->firmware_ctrl, 0);
				else
					facs = NULL;
				return facs;
			}
		}
	}

	return NULL;
}

void *acpi_get_end(void)
{
	const struct acpi_table_header *end;
	const struct acpi_rsdt *rsdt;
	const struct acpi_xsdt *xsdt;
	bool has_xsdt;
	int i, count;
	void *ptr;

	count = setup_search(&has_xsdt, NULL, &ptr);
	if (!count)
		return NULL;
	xsdt = ptr;
	rsdt = ptr;

	end = has_xsdt ? &xsdt->header : &rsdt->header;
	for (i = 0; i < count; i++) {
		const struct acpi_table_header *hdr;

		if (has_xsdt)
			hdr = nomap_sysmem(xsdt->entry[i], 0);
		else
			hdr = nomap_sysmem(rsdt->entry[i], 0);
		end = max(hdr, end);
	}

	return (void *)end + end->length;
}

void acpi_fill_header(struct acpi_table_header *header, char *signature)
{
	memcpy(header->signature, signature, 4);
	memcpy(header->oem_id, OEM_ID, 6);
	memcpy(header->oem_table_id, OEM_TABLE_ID, 8);
	header->oem_revision = OEM_REVISION;
	memcpy(header->creator_id, ASLC_ID, 4);
	header->creator_revision = ASL_REVISION;
}

void acpi_align(struct acpi_ctx *ctx)
{
	ctx->current = (void *)ALIGN((ulong)ctx->current, 16);
}

void acpi_align64(struct acpi_ctx *ctx)
{
	ctx->current = (void *)ALIGN((ulong)ctx->current, 64);
}

void acpi_inc(struct acpi_ctx *ctx, uint amount)
{
	ctx->current += amount;
}

void acpi_inc_align(struct acpi_ctx *ctx, uint amount)
{
	ctx->current += amount;
	acpi_align(ctx);
}

/**
 * Add an ACPI table to the RSDT (and XSDT) structure, recalculate length
 * and checksum.
 */
int acpi_add_table(struct acpi_ctx *ctx, void *table)
{
	int i, entries_num;
	struct acpi_rsdt *rsdt;
	struct acpi_xsdt *xsdt;

	/* On legacy x86 platforms the RSDT is mandatory while the XSDT is not.
	 * On other platforms there might be no memory below 4GiB, thus RSDT is NULL.
	 */
	if (ctx->rsdt) {
		rsdt = ctx->rsdt;

		/* This should always be MAX_ACPI_TABLES */
		entries_num = ARRAY_SIZE(rsdt->entry);

		for (i = 0; i < entries_num; i++) {
			if (rsdt->entry[i] == 0)
				break;
		}

		if (i >= entries_num) {
			log_err("ACPI: Error: too many tables\n");
			return -E2BIG;
		}

		/* Add table to the RSDT */
		rsdt->entry[i] = nomap_to_sysmem(table);

		/* Fix RSDT length or the kernel will assume invalid entries */
		rsdt->header.length = sizeof(struct acpi_table_header) +
					(sizeof(u32) * (i + 1));

		/* Re-calculate checksum */
		acpi_update_checksum(&rsdt->header);
	}

	if (ctx->xsdt) {
		/*
		 * And now the same thing for the XSDT. We use the same index as for
		 * now we want the XSDT and RSDT to always be in sync in U-Boot
		 */
		xsdt = ctx->xsdt;

		/* This should always be MAX_ACPI_TABLES */
		entries_num = ARRAY_SIZE(xsdt->entry);

		for (i = 0; i < entries_num; i++) {
			if (xsdt->entry[i] == 0)
				break;
		}

		if (i >= entries_num) {
			log_err("ACPI: Error: too many tables\n");
			return -E2BIG;
		}

		/* Add table to the XSDT */
		xsdt->entry[i] = nomap_to_sysmem(table);

		/* Fix XSDT length */
		xsdt->header.length = sizeof(struct acpi_table_header) +
					(sizeof(u64) * (i + 1));

		/* Re-calculate checksum */
		acpi_update_checksum(&xsdt->header);
	}

	return 0;
}
