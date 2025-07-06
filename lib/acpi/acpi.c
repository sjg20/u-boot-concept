// SPDX-License-Identifier: GPL-2.0
/*
 * Utility functions for ACPI
 *
 * Copyright 2023 Google LLC
 */

#define LOG_CATEGORY	LOGC_ACPI

#include <errno.h>
#include <mapmem.h>
#include <tables_csum.h>
#include <acpi/acpi_table.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

void acpi_update_checksum(struct acpi_table_header *header)
{
	header->checksum = 0;
	header->checksum = table_compute_checksum(header, header->length);
}

static bool acpi_valid_rsdp(struct acpi_rsdp *rsdp)
{
	if (strncmp((char *)rsdp, RSDP_SIG, sizeof(RSDP_SIG) - 1) != 0)
		return false;

	debug("Looking on %p for valid checksum\n", rsdp);

	if (table_compute_checksum((void *)rsdp, 20) != 0)
		return false;
	debug("acpi rsdp checksum 1 passed\n");

	if (rsdp->revision > 1 &&
	    table_compute_checksum((void *)rsdp, rsdp->length))
		return false;
	debug("acpi rsdp checksum 2 passed\n");

	return true;
}

/**
 * setup_search() - Set up for searching through the RSDT/XSDT
 *
 * Looks for XSDT first and uses those entries if available, else RSDT
 *
 * @rsdtp: Returns RSDT if present
 * @xsdtp: Returns XSDT if present
 * Return: number of entries in table, -ENOENT if there is no RSDP, -EINVAL if
 *	the RSDP is invalid, -ENOTSYNC if both tables exist and their counts
 *	disagree
 */
static int setup_search(struct acpi_rsdt **rsdtp, struct acpi_xsdt **xsdtp)
{
	struct acpi_rsdp *rsdp;
	struct acpi_rsdt *rsdt = NULL;
	struct acpi_xsdt *xsdt = NULL;
	int len, count = 0;


	rsdp = map_sysmem(gd_acpi_start(), 0);
	if (!rsdp)
		return -ENOENT;
	if (!acpi_valid_rsdp(rsdp))
		return -EINVAL;
	if (rsdp->xsdt_address) {
		xsdt = nomap_sysmem(rsdp->xsdt_address, 0);
		len = xsdt->header.length - sizeof(xsdt->header);
		count = len / sizeof(u64);
	}

	if (rsdp->rsdt_address) {
		int rcount;

		rsdt = nomap_sysmem(rsdp->rsdt_address, 0);
		len = rsdt->header.length - sizeof(rsdt->header);
		rcount = len / sizeof(u32);
		if (xsdt) {
			if (rcount != count)
				return -ENOTSYNC;
		} else {
			count = rcount;
		}
	}
	*rsdtp = rsdt;
	*xsdtp = xsdt;

	return count;
}

struct acpi_table_header *acpi_find_table(const char *sig)
{
	struct acpi_rsdt *rsdt;
	struct acpi_xsdt *xsdt;
	int i, count, ret;

	ret = setup_search(&rsdt, &xsdt);
	if (ret < 0) {
		log_warning("acpi: Failed to find tables (err=%d)\n", ret);
		return NULL;
	}
	if (!ret)
		return NULL;
	count = ret;

	for (i = 0; i < count; i++) {
		struct acpi_table_header *hdr;

		if (xsdt)
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
