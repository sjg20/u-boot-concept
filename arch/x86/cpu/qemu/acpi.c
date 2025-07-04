// SPDX-License-Identifier: GPL-2.0+
/*
 * QEMU x86-specific ACPI handling
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <errno.h>
#include <log.h>
#include <acpi/acpi_table.h>
#include <asm/acpi.h>
#include <asm/byteorder.h>
#include <linux/types.h>

/**
 * cmd_acpinvs.c
 *
 * U-Boot command to parse the ACPI DSDT to find the Non-Volatile Storage (NVS)
 * region defined as a SystemMemory OperationRegion.
 *
 * This involves a recursive AML (ACPI Machine Language) parser capable of
 * descending into ScopeOp blocks to find the target.
 */

/* AML Opcodes used for parsing */
#define AML_EXT_OP_PREFIX   0x5B
#define AML_OPREGION_OP     0x80
#define AML_SCOPE_OP        0x10

/*
 * Represents a found NVS region.
 * Using a struct to easily pass results back.
 */
struct nvs_info {
	u64 addr;
	u64 size;
	bool found;
};

/* Forward declaration for the recursive parser */
static int find_nvs_in_aml_block(const u8 *aml, const u8 *aml_end,
				 const u8 *dsdt_start, struct nvs_info *nvs);

/**
 * aml_get_pkg_length() - Decodes the PkgLength field from an AML stream.
 *
 * The PkgLength encoding is variable-length. The top 2 bits of the first
 * byte determine how many subsequent bytes make up the length field.
 *
 * @aml: Pointer to the start of the PkgLength field.
 * @pkg_len: Pointer to store the decoded package length.
 *
 * Returns: The number of bytes consumed by the PkgLength field itself.
 */
static int aml_get_pkg_length(const u8 *aml, u32 *pkg_len)
{
	u8 byte_count = (aml[0] >> 6) & 0x03;
	u32 len = 0;

	if (byte_count == 0) {
		*pkg_len = aml[0] & 0x3F;
		return 1;
	}

	/* The length is encoded in the lower nibble of the first byte */
	/* and all of the subsequent bytes. */
	len = aml[0] & 0x0F;
	for (int i = 0; i < byte_count; i++) {
		len |= (u32)aml[i + 1] << (4 + i * 8);
	}

	*pkg_len = len;
	return byte_count + 1;
}

/**
 * aml_parse_integer() - Parses an AML integer constant.
 *
 * AML integers are prefixed with an opcode indicating their size.
 *
 * @aml: Pointer to the integer opcode.
 * @val: Pointer to store the parsed integer value.
 *
 * Returns: The number of bytes consumed by the integer term (opcode + data).
 */
static int aml_parse_integer(const u8 *aml, u64 *val)
{
	switch (aml[0]) {
	case 0x00: /* ZeroOp */
		*val = 0;
		return 1;
	case 0x01: /* OneOp */
		*val = 1;
		return 1;
	case 0xFF: /* OnesOp */
		*val = -1ULL;
		return 1;
	case 0x0A: /* BytePrefix */
		*val = aml[1];
		return 2;
	case 0x0B: /* WordPrefix */
		*val = le16_to_cpup((const u16 *)&aml[1]);
		return 3;
	case 0x0C: /* DWordPrefix */
		*val = le32_to_cpup((const u32 *)&aml[1]);
		return 5;
	case 0x0E: /* QWordPrefix */
		*val = le64_to_cpup((const u64 *)&aml[1]);
		return 9;
	default:
		/* Not a simple integer, could be a NameString or complex term. */
		/* This simple parser doesn't handle that. */
		return -1;
	}
}

/**
 * find_nvs_in_aml_block() - Recursively walks an AML block to find the NVS.
 *
 * @aml: Pointer to the start of the current AML block to parse.
 * @aml_end: Pointer to the end of the current AML block.
 * @dsdt_start: Pointer to the absolute start of the DSDT's AML (for offsets).
 * @nvs: Pointer to a struct to store the results.
 *
 * Returns: 0 on success (found), -1 on failure (not found in this block).
 */
static int find_nvs_in_aml_block(const u8 *aml, const u8 *aml_end,
				 const u8 *dsdt_start, struct nvs_info *nvs)
{
	const u8 *p = aml;
	int ret;

	while (p < aml_end) {
		if (p[0] == AML_EXT_OP_PREFIX && p[1] == AML_OPREGION_OP) {
			const u8 *op = p;
			const char *name = (const char *)&op[2];
			u8 region_space = op[6];

			log_info("Found OperationRegion '%.4s' at offset 0x%lx\n",
			       name, (ulong)(op - dsdt_start));

			if (region_space == 0x00) { /* SystemMemory */
				int offset_len, size_len;

				log_info("  -> Region is SystemMemory. This is a candidate for NVS.\n");

				/* Parse the address and size */
				offset_len = aml_parse_integer(&op[7], &nvs->addr);
				if (offset_len < 0) {
					log_info("  -> ERROR: Could not parse RegionOffset.\n");
					p++; /* Skip and continue scan */
					continue;
				}

				size_len = aml_parse_integer(&op[7 + offset_len], &nvs->size);
				if (size_len < 0) {
					log_info("  -> ERROR: Could not parse RegionLength.\n");
					p++; /* Skip and continue scan */
					continue;
				}

				log_info("  -> NVS Found: Address=0x%llx, Size=0x%llx\n",
				       nvs->addr, nvs->size);
				nvs->found = true;
				return 0; /* Success */
			}
			p++; /* Not SystemMemory, continue scan */

		} else if (p[0] == AML_SCOPE_OP) {
			const u8 *scope_op = p;
			u32 pkg_len;
			/* PkgLength field follows the ScopeOp opcode */
			int len_bytes = aml_get_pkg_length(&scope_op[1], &pkg_len);
			/* The next op starts after the entire ScopeOp package */
			const u8 *next_op = scope_op + pkg_len;
			const u8 *scope_content_start;

			/* Sanity check to prevent infinite loops on malformed tables */
			if (next_op <= scope_op) {
				log_info("Warning: Invalid ScopeOp length at offset 0x%lx. Skipping one byte.\n",
				       (ulong)(scope_op - dsdt_start));
				p++;
				continue;
			}

			if (next_op > aml_end) {
				log_info("Error: ScopeOp at offset 0x%lx has length %u which exceeds current parsing limit.\n",
				       (ulong)(scope_op - dsdt_start), pkg_len);
				p = next_op; /* Skip to end to avoid overflow */
				continue;
			}

			log_info("Descending into ScopeOp at offset 0x%lx, length %u\n",
			       (ulong)(scope_op - dsdt_start), pkg_len);

			/* The content of the scope starts after the PkgLength field and the 4-byte NameString */
			scope_content_start = scope_op + 1 + len_bytes + 4;
			if (scope_content_start < next_op) {
				ret = find_nvs_in_aml_block(scope_content_start, next_op,
							    dsdt_start, nvs);
				if (ret == 0)
					return 0; /* Found in nested scope */
			}

			/* Not found, so skip the entire scope block */
			p = next_op;
		} else {
			p++; /* Move to the next byte to continue scanning */
		}
	}

	return -1; /* Not found in this block */
}

/**
 * find_nvs_in_dsdt() - Top-level wrapper to start the DSDT parsing.
 *
 * @dsdt: Pointer to the DSDT header.
 * @nvs: Pointer to a struct to store the results.
 *
 * Returns: 0 on success, -1 on failure.
 */
static int find_nvs_in_dsdt(struct acpi_table_header *dsdt, struct nvs_info *nvs)
{
	const u8 *aml_start = (const u8 *)dsdt + sizeof(struct acpi_table_header);
	const u8 *aml_end = (const u8 *)dsdt + dsdt->length;

	nvs->found = false;

	log_info("Parsing DSDT at 0x%p, length %u bytes\n", dsdt, dsdt->length);

	return find_nvs_in_aml_block(aml_start, aml_end, aml_start, nvs);
}

int acpi_find_nvs(ulong *addrp, ulong *sizep)
{
	struct acpi_table_header *dsdt;
	struct nvs_info nvs;
	int ret;

	/* Find the DSDT table using U-Boot's ACPI helpers */
	dsdt = acpi_find_table("DSDT");
	if (!dsdt) {
		printf("Error: DSDT table not found.\n");
		return -ENOENT;
	}

	/* Parse the found DSDT for the NVS region */
	ret = find_nvs_in_dsdt(dsdt, &nvs);
	if (ret == 0 && nvs.found) {
		printf("\nSuccess! Found NVS region.\n");
		printf("Address: 0x%llx\n", nvs.addr);
		printf("Size:    0x%llx (%llu bytes)\n", nvs.size, nvs.size);
		*addrp = nvs.addr;
		*sizep = nvs.size;
		return 0;
	}

	return 0;
}
