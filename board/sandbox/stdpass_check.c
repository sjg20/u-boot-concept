// SPDX-License-Identifier: GPL-2.0+
/*
 * This file compiles all the struct definitions for standard passage, to ensure
 * there are no errors
 *
 * Copyright 2021 Google LLC
 */

#include <common.h>

/*
 * See also doc/develop/std_passage.rst
 *
 * Instructions:
 *
 * 1. Add your header file to U-Boot, or to include/stdpass if it is not used in
 * U-Boot
 *
 * 2. Add a function below to include the header and use the struct. Please put
 * your function in order of tag ID (see bloblist.h)
 *
 * Template follows, see above for example
 */

/* BLOBLISTT_tag here */
/* #include <stdpass/yourfile.h> if not used in U-Boot*/
void check_struct_name(void)
{
	/* __maybe_unused struct struct_name check; */
}

/* BLOBLISTT_CONTROL_DTB */
void check_control_dtb(void)
{
	/*
	 * Defined by devicetree specification
	 * https://github.com/devicetree-org/devicetree-specification/releases/tag/v0.3
	 */
};

/* BLOBLISTT_ACPI_GNVS */
#include <intel_gnvs.h>
void check_acpi_gnvs(void)
{
	__maybe_unused struct acpi_global_nvs check;
}

/* BLOBLISTT_INTEL_VBT */
void check_intel_vbt(void)
{
	/*
	 * Pre-existing Intel blob, defined by source code
	 *
	 * https://github.com/freedesktop/xorg-intel-gpu-tools/blob/master/tools/intel_vbt_defs.h
	 * https://github.com/freedesktop/xorg-intel-gpu-tools/blob/master/tools/intel_vbt_decode.c
	 */
}

/* BLOBLISTT_TPM2_TCG_LOG */
#include <stdpass/tpm2_eventlog.h>
void check_tpm2_tcg_log(void)
{
	/* Struct for each record */
	__maybe_unused struct tpm2_eventlog_context check;
}

/* BLOBLISTT_TCPA_LOG */
#include <acpi/acpi_table.h>
void check_tcpa_log(void)
{
	__maybe_unused struct acpi_tcpa check;
};

/* BLOBLISTT_ACPI_TABLES */
void check_acpi_tables(void)
{
	/*
	 * Defined by UEFI Advanced Configuration and Power Interface (ACPI)
	 * Specification, Version 6.3, January 2019
	 * https://uefi.org/sites/default/files/resources/ACPI_6_3_final_Jan30.pdf
	 */
}

/* BLOBLISTT_SMBIOS_TABLES */
void check_smbios_tables(void)
{
	/*
	 * Defined by System Management BIOS (SMBIOS) Reference Specification
	 * v3.5.0
	 * https://www.dmtf.org/standards/smbios
	 */
}

/* BLOBLISTT_VBOOT_CTX */
#include <stdpass/vboot_ctx.h>
void check_vboot_ctx(void)
{
	__maybe_unused struct vb2_shared_data check;

}

/* BLOBLISTT_U_BOOT_SPL_HANDOFF */
#include <handoff.h>
void check_spl_handoff(void)
{
	__maybe_unused struct spl_handoff check;
};
