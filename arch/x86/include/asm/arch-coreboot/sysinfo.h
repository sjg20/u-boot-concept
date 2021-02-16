/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * This file is taken from coreboot sysinfo.h
 *
 * Copyright (C) 2008 Advanced Micro Devices, Inc.
 */

#ifndef _COREBOOT_SYSINFO_H
#define _COREBOOT_SYSINFO_H

/* Maximum number of memory range definitions. */
#define SYSINFO_MAX_MEM_RANGES 32

/* Allow a maximum of 8 GPIOs */
#define SYSINFO_MAX_GPIOS 8

/* Up to 10 MAC addresses */
#define SYSINFO_MAX_MACS 10

#include <asm/coreboot_tables.h>

struct cb_serial;

struct sysinfo_t {
	unsigned int cpu_khz;
	struct cb_serial *serial;
	unsigned short ser_ioport;
	unsigned long ser_base; // for mmapped serial

	int n_memranges;

	struct memrange {
		unsigned long long base;
		unsigned long long size;
		unsigned int type;
	} memrange[SYSINFO_MAX_MEM_RANGES];

	struct cb_cmos_option_table *option_table;
	u32 cmos_range_start;
	u32 cmos_range_end;
	u32 cmos_checksum_location;
	u32 vbnv_start;
	u32 vbnv_size;

	char *version;
	char *extra_version;
	char *build;
	char *compile_time;
	char *compile_by;
	char *compile_host;
	char *compile_domain;
	char *compiler;
	char *linker;
	char *assembler;

	char *cb_version;

	struct cb_framebuffer *framebuffer;

	int num_gpios;
	struct cb_gpio gpios[SYSINFO_MAX_GPIOS];
	int num_macs;
	struct mac_address macs[SYSINFO_MAX_MACS];
	char *serialno;

	unsigned long *mbtable; /** Pointer to the multiboot table */

	struct cb_header *header;
	struct cb_mainboard *mainboard;

	void	*vboot_handoff;
	u32	vboot_handoff_size;
	void	*vdat_addr;
	u32	vdat_size;
	u64 smbios_start;
	u32 smbios_size;

	int x86_rom_var_mtrr_index;

	void		*tstamp_table;
	void		*cbmem_cons;
	void		*mrc_cache;
	void		*acpi_gnvs;
	u32		board_id;
	u32		ram_code;
	void		*wifi_calibration;
	uint64_t	ramoops_buffer;
	uint32_t	ramoops_buffer_size;
	struct {
		uint32_t size;
		uint32_t sector_size;
		uint32_t erase_cmd;
	} spi_flash;
	uint64_t fmap_offset;
	uint64_t cbfs_offset;
	uint64_t cbfs_size;
	uint64_t boot_media_size;
	uint64_t mtc_start;
	uint32_t mtc_size;
	void	*chromeos_vpd;
};

extern struct sysinfo_t lib_sysinfo;

int get_coreboot_info(struct sysinfo_t *info);

#endif
