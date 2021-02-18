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

struct timestamp_entry {
	u32	entry_id;
	u64	entry_stamp;
} __packed;

struct timestamp_table {
	u64	base_time;
	u16	max_entries;
	u16	tick_freq_mhz;
	u32	num_entries;
	struct timestamp_entry entries[0]; /* Variable number of entries */
} __packed;

enum timestamp_id {
	TS_START_ROMSTAGE = 1,
	TS_BEFORE_INITRAM = 2,
	TS_AFTER_INITRAM = 3,
	TS_END_ROMSTAGE = 4,
	TS_START_VBOOT = 5,
	TS_END_VBOOT = 6,
	TS_START_COPYRAM = 8,
	TS_END_COPYRAM = 9,
	TS_START_RAMSTAGE = 10,
	TS_START_BOOTBLOCK = 11,
	TS_END_BOOTBLOCK = 12,
	TS_START_COPYROM = 13,
	TS_END_COPYROM = 14,
	TS_START_ULZMA = 15,
	TS_END_ULZMA = 16,
	TS_START_ULZ4F = 17,
	TS_END_ULZ4F = 18,
	TS_DEVICE_ENUMERATE = 30,
	TS_DEVICE_CONFIGURE = 40,
	TS_DEVICE_ENABLE = 50,
	TS_DEVICE_INITIALIZE = 60,
	TS_DEVICE_DONE = 70,
	TS_CBMEM_POST = 75,
	TS_WRITE_TABLES = 80,
	TS_FINALIZE_CHIPS = 85,
	TS_LOAD_PAYLOAD = 90,
	TS_ACPI_WAKE_JUMP = 98,
	TS_SELFBOOT_JUMP = 99,

	/* 500+ reserved for vendorcode extensions (500-600: google/chromeos) */
	TS_START_COPYVER = 501,
	TS_END_COPYVER = 502,
	TS_START_TPMINIT = 503,
	TS_END_TPMINIT = 504,
	TS_START_VERIFY_SLOT = 505,
	TS_END_VERIFY_SLOT = 506,
	TS_START_HASH_BODY = 507,
	TS_DONE_LOADING = 508,
	TS_DONE_HASHING = 509,
	TS_END_HASH_BODY = 510,
	TS_START_COPYVPD = 550,
	TS_END_COPYVPD_RO = 551,
	TS_END_COPYVPD_RW = 552,

	/* 940-950 reserved for vendorcode extensions (940-950: Intel ME) */
	TS_ME_INFORM_DRAM_WAIT = 940,
	TS_ME_INFORM_DRAM_DONE = 941,

	/* 950+ reserved for vendorcode extensions (950-999: intel/fsp) */
	TS_FSP_MEMORY_INIT_START = 950,
	TS_FSP_MEMORY_INIT_END = 951,
	TS_FSP_TEMP_RAM_EXIT_START = 952,
	TS_FSP_TEMP_RAM_EXIT_END = 953,
	TS_FSP_SILICON_INIT_START = 954,
	TS_FSP_SILICON_INIT_END = 955,
	TS_FSP_BEFORE_ENUMERATE = 956,
	TS_FSP_AFTER_ENUMERATE = 957,
	TS_FSP_BEFORE_FINALIZE = 958,
	TS_FSP_AFTER_FINALIZE = 959,
	TS_FSP_BEFORE_END_OF_FIRMWARE = 960,
	TS_FSP_AFTER_END_OF_FIRMWARE = 961,

	/* 1000+ reserved for payloads (1000-1200: ChromeOS depthcharge) */

	/* Depthcharge entry IDs start at 1000 */
	TS_DC_START = 1000,

	TS_RO_PARAMS_INIT = 1001,
	TS_RO_VB_INIT = 1002,
	TS_RO_VB_SELECT_FIRMWARE = 1003,
	TS_RO_VB_SELECT_AND_LOAD_KERNEL = 1004,

	TS_RW_VB_SELECT_AND_LOAD_KERNEL = 1010,

	TS_VB_SELECT_AND_LOAD_KERNEL = 1020,
	TS_VB_EC_VBOOT_DONE = 1030,
	TS_VB_STORAGE_INIT_DONE = 1040,
	TS_VB_READ_KERNEL_DONE = 1050,
	TS_VB_VBOOT_DONE = 1100,

	TS_START_KERNEL = 1101,
	TS_KERNEL_DECOMPRESSION = 1102,
};

struct cbmem_console {
	u32 size;
	u32 cursor;
	char body[0];
} __packed;

/**
 * struct sysinfo_t - Information passed to U-Boot from coreboot
 *
 * Coreboot passes on a lot of information using a list of individual data
 * structures identified by a numeric tag. These are parsed in U-Boot to produce
 * this struct. Some of the pointers here point back to the tagged data
 * structure, since it is assumed to remain around while U-Boot is running.
 *
 * The 'cbsysinfo' command can display this information.
 *
 * @cpu_khz: CPU frequence in KHz (e.g. 1100000)
 * @serial: Pointer to the serial information, NULL if none
 * @ser_ioport: Not actually provided by a tag and not used on modern hardware,
 *	which typicaally uses a memory-mapped port
 * @ser_base: Not used at all, but present to match up with the coreboot data
 *	structure
 * @n_memranges: Number of memory ranges
 * @memrange: List of memory ranges:
 *	@base: Base address of range
 *	@size: Size of range in bytes
 *	@type: Type of range (CB_MEM_RAM, etc.)
 * @option_table: Provides a pointer to the CMOS RAM options table, which
 *	indicates which options are available. The header is followed by a list
 *	of struct cb_cmos_entries records, so that an option can be found from
 *	its name. This is not used in U-Boot. NULL if not present
 * @cmos_range_start: Start bit of the CMOS checksum range (in fact this must
 *	be a multiple of 8)
 * @cmos_range_end: End bit of the CMOS checksum range (multiple of 8). This is
 *	the inclusive end.
 * @cmos_checksum_location: Location of checksum, multiplied by 8. This is the
 *	byte offset into the CMOS RAM of the first checksum byte. The second one
 *	follows immediately. The checksum is a simple 16-bit sum of all the
 *	bytes from offset cmos_range_start / 8 to cmos_range_end / 8, inclusive,
 *	in big-endian format (so sum >> 8 is stored in the first byte).
 * @vbnv_start: Start offset of CMOS RAM used for Chromium OS verified boot
 *	(typically 0x34)
 * @vbnv_size: Number of bytes used by Chromium OS verified boot (typically
 *	0x10)
 * @extra_version: Extra version information, typically ""
 * @build: Build date, e.g. "Wed Nov 18 02:51:58 UTC 2020"
 * @compile_time: Compilation time, e.g. "02:51:58"
 * @compile_by: Who compiled coreboot (never set?)
 * @compile_host: Name of the machine that compiled coreboot (never set?)
 * @compile_domain: Domain name of the machine that compiled coreboot (never
 *	set?)
 * @compiler: Name of the compiler used to build coreboot (never set?)
 * @linker: Name of the linker used to build coreboot (never set?)
 * @assembler: Name of the assembler used to build coreboot (never set?)
 * @cb_version: Coreboot version string, e.g. v1.9308_26_0.0.22-2599-g232f22c75d
 * @framebuffer: Address of framebuffer tag, or NULL if none. See
 *	struct cb_framebuffer for the definition
 * @num_gpios: Number of verified-boot GPIOs
 * @gpios: List of GPIOs:
 *	@port: GPIO number, or 0xffffffff if not a GPIO
 *	@polarity: CB_GPIO_ACTIVE_LOW or CB_GPIO_ACTIVE_HIGH
 *	@value: Value of GPIO (0 or 1)
 *	@name: Name of GPIO
 *
 *	A typical list is:
 *	  id: port     polarity val name
 *	   0:    -  active-high   1 write protect
 *	   1:    -  active-high   0 recovery
 *	   2:    -  active-high   1 lid
 *	   3:    -  active-high   0 power
 *	   4:    -  active-high   0 oprom
 *	   5:   29  active-high   0 EC in RW
 *
 * @num_macs: Number of MAC addresses
 * @macs: List of MAC addresses
 * @serialno: Serial number, or NULL (never set?)
 * @mbtable: Adress of the multiboot table, or NULL. This is a
 *	struct multiboot_header, not used in U-Boot
 * @header: Address of header, if there is a CB_TAG_FORWARD, else NULL
 * @mainboard: Pointer to mainboard info or NULL. Typically the vendor is
 *	"Google" and the part number is ""
 * @vboot_handoff: Pointer to Chromium OS verified boot hand-off information.
 *	This is struct vboot_handoff, providing access to internal information
 *	generated by coreboot when this is being used
 * @vboot_handoff_size: Size of hand-off information (typically 0xc0c)
 * @vdat_addr: Pointer to Chromium OS verified boot data, which uses
 *	struct chromeos_acpi. It sits in the Intel Global NVS struct, after the
 *	first 0x100 bytes
 * @vdat_size: Size of this data, typically 0xf00
 * @smbios_start: Address of SMBIOS tables
 * @smbios_size: Size of SMBIOS tables (e.g. 0x800)
 * @x86_rom_var_mtrr_index: MTRR number used for ROM caching. Not used in U-Boot
 * @tstamp_table: Pointer to timestamp_table, struct timestamp_table
 * @cbmem_cons: Pointer to the console dump, struct cbmem_console. This provides
 *	access to the console output generated by coreboot, typically about 64KB
 *	and mostly PCI enumeration info
 * @mrc_cache: Pointer to memory-reference-code cache, typically NULL
 * acpi_gnvs: @Pointer to Intel Global NVS struct, see struct acpi_global_nvs
 * @board_id: Board ID indicating the board variant, typically 0xffffffff
 * @ram_code: RAM code indicating the SDRAM type, typically 0xffffffff
 * @wifi_calibration: WiFi calibration info, NULL if none
 * @ramoops_buffer: Address of kernel Ramoops buffer
 * @ramoops_buffer_size: Sizeof of Ramoops buffer, typically 1MB
 * @spi_flash: Information about SPI flash:
 *	@size: Size in bytes, e.g. 16MB
 *	@sector_size; Sector size of flash device, e.g. 4KB
 *	@erase_cmd: Command used to erase flash, or 0 if not used
 * @fmap_offset: SPI-flash offset of the flash map (FMAP) table. This has a
 *	__FMAP__ header. It provides information about the different top-level
 *	sections in the SPI flash, e.g. 0x204000
 * @cbfs_offset: SPI-flash offset of the Coreboot Filesystem (CBFS) used for
 *	read-only data, e.g. 0x205000. This is typically called 'COREBOOT' in
 *	the flash map. It holds various coreboot binaries as well as
 *	video-configuration files and graphics data for the Chromium OS
 *	verified boot user interface.
 * @cbfs_size: Size of CBFS, e.g. 0x17b000
 * @boot_media_size; Size of boot media (i.e. SPI flash), e.g. 16MB
 * @mtc_start; Start of MTC region (Nvidia private data), 0 if not used. See
 *	https://chromium.googlesource.com/chromiumos/third_party/coreboot/+/chromeos-2013.04/src/soc/nvidia/tegra210/mtc.c
 * @mtc_size: Size of MTC region
 * @chromeos_vpd: Chromium OS Vital Product Data region, typically NULL, meaning
 *	not used
 */
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
