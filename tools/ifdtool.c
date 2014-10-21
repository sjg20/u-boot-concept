/*
 * ifdtool - Manage Intel Firmware Descriptor information
 *
 * Copyright (C) 2011 The ChromiumOS Authors.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * From Coreboot project, but it got a serious code clean-up
 */

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ifdtool.h"

#undef DEBUG

#ifdef DEBUG
#define debug(fmt,args...)	printf(fmt ,##args)
#else
#define debug(fmt,args...)
#endif

#define FD_SIGNATURE		0x0FF0A55A
#define FLREG_BASE(reg)		((reg & 0x00000fff) << 12);
#define FLREG_LIMIT(reg)	(((reg & 0x0fff0000) >> 4) | 0xfff);

static struct fdbar_t *find_fd(char *image, int size)
{
	uint32_t *ptr, *end;

	/* Scan for FD signature */
	for (ptr = (uint32_t *)image, end = ptr + size / 4; ptr < end; ptr++) {
		if (*ptr == FD_SIGNATURE)
			break;
	}

	if (ptr == end) {
		printf("No Flash Descriptor found in this image\n");
		return NULL;
	}

	debug("Found Flash Descriptor signature at 0x%08x\n", i);

	return (struct fdbar_t *)ptr;
}

static int get_region(struct frba_t *frba, int region_type,
		      struct region_t *region)
{
	if (region_type >= 5) {
		fprintf(stderr, "Invalid region type.\n");
		return -1;
	}

	region->base = FLREG_BASE(frba->flreg[region_type]);
	region->limit = FLREG_LIMIT(frba->flreg[region_type]);
	region->size = region->limit - region->base + 1;

	return 0;
}

static const char *region_name(int region_type)
{
	static const char *regions[] = {
		"Flash Descriptor",
		"BIOS",
		"Intel ME",
		"GbE",
		"Platform Data"
	};

	assert(region_type < MAX_REGIONS);

	return regions[region_type];
}

static const char *region_filename(int region_type)
{
	static const char *region_filenames[] = {
		"flashregion_0_flashdescriptor.bin",
		"flashregion_1_bios.bin",
		"flashregion_2_intel_me.bin",
		"flashregion_3_gbe.bin",
		"flashregion_4_platform_data.bin"
	};

	assert(region_type < MAX_REGIONS);

	return region_filenames[region_type];
}

static int dump_region(int num, struct frba_t *frba)
{
	struct region_t region;
	int ret;

	ret = get_region(frba, num, &region);
	if (ret)
		return ret;

	printf("  Flash Region %d (%s): %08x - %08x %s\n",
		       num, region_name(num), region.base, region.limit,
		       region.size < 1 ? "(unused)" : "");

	return ret;
}

static void dump_frba(struct frba_t *frba)
{
	int i;

	printf("Found Region Section\n");
	for (i = 0; i < MAX_REGIONS; i++) {
		printf("FLREG%d:    0x%08x\n", i, frba->flreg[i]);
		dump_region(i, frba);
	}
}

static void decode_spi_frequency(unsigned int freq)
{
	switch (freq) {
	case SPI_FREQUENCY_20MHZ:
		printf("20MHz");
		break;
	case SPI_FREQUENCY_33MHZ:
		printf("33MHz");
		break;
	case SPI_FREQUENCY_50MHZ:
		printf("50MHz");
		break;
	default:
		printf("unknown<%x>MHz", freq);
	}
}

static void decode_component_density(unsigned int density)
{
	switch (density) {
	case COMPONENT_DENSITY_512KB:
		printf("512KB");
		break;
	case COMPONENT_DENSITY_1MB:
		printf("1MB");
		break;
	case COMPONENT_DENSITY_2MB:
		printf("2MB");
		break;
	case COMPONENT_DENSITY_4MB:
		printf("4MB");
		break;
	case COMPONENT_DENSITY_8MB:
		printf("8MB");
		break;
	case COMPONENT_DENSITY_16MB:
		printf("16MB");
		break;
	default:
		printf("unknown<%x>MB", density);
	}
}

static void dump_fcba(struct fcba_t *fcba)
{
	printf("\nFound Component Section\n");
	printf("FLCOMP     0x%08x\n", fcba->flcomp);
	printf("  Dual Output Fast Read Support:       %ssupported\n",
		(fcba->flcomp & (1 << 30))?"":"not ");
	printf("  Read ID/Read Status Clock Frequency: ");
	decode_spi_frequency((fcba->flcomp >> 27) & 7);
	printf("\n  Write/Erase Clock Frequency:         ");
	decode_spi_frequency((fcba->flcomp >> 24) & 7);
	printf("\n  Fast Read Clock Frequency:           ");
	decode_spi_frequency((fcba->flcomp >> 21) & 7);
	printf("\n  Fast Read Support:                   %ssupported",
		(fcba->flcomp & (1 << 20))?"":"not ");
	printf("\n  Read Clock Frequency:                ");
	decode_spi_frequency((fcba->flcomp >> 17) & 7);
	printf("\n  Component 2 Density:                 ");
	decode_component_density((fcba->flcomp >> 3) & 7);
	printf("\n  Component 1 Density:                 ");
	decode_component_density(fcba->flcomp & 7);
	printf("\n");
	printf("FLILL      0x%08x\n", fcba->flill);
	printf("  Invalid Instruction 3: 0x%02x\n",
		(fcba->flill >> 24) & 0xff);
	printf("  Invalid Instruction 2: 0x%02x\n",
		(fcba->flill >> 16) & 0xff);
	printf("  Invalid Instruction 1: 0x%02x\n",
		(fcba->flill >> 8) & 0xff);
	printf("  Invalid Instruction 0: 0x%02x\n",
		fcba->flill & 0xff);
	printf("FLPB       0x%08x\n", fcba->flpb);
	printf("  Flash Partition Boundary Address: 0x%06x\n\n",
		(fcba->flpb & 0xfff) << 12);
}

static void dump_fpsba(struct fpsba_t *fpsba)
{
	int i;

	printf("Found PCH Strap Section\n");
	for (i = 0; i < MAX_STRAPS; i++)
		printf("PCHSTRP%-2d:  0x%08x\n", i, fpsba->pchstrp[i]);
}

static void decode_flmstr(uint32_t flmstr)
{
	printf("  Platform Data Region Write Access: %s\n",
		(flmstr & (1 << 28)) ? "enabled" : "disabled");
	printf("  GbE Region Write Access:           %s\n",
		(flmstr & (1 << 27)) ? "enabled" : "disabled");
	printf("  Intel ME Region Write Access:      %s\n",
		(flmstr & (1 << 26)) ? "enabled" : "disabled");
	printf("  Host CPU/BIOS Region Write Access: %s\n",
		(flmstr & (1 << 25)) ? "enabled" : "disabled");
	printf("  Flash Descriptor Write Access:     %s\n",
		(flmstr & (1 << 24)) ? "enabled" : "disabled");

	printf("  Platform Data Region Read Access:  %s\n",
		(flmstr & (1 << 20)) ? "enabled" : "disabled");
	printf("  GbE Region Read Access:            %s\n",
		(flmstr & (1 << 19)) ? "enabled" : "disabled");
	printf("  Intel ME Region Read Access:       %s\n",
		(flmstr & (1 << 18)) ? "enabled" : "disabled");
	printf("  Host CPU/BIOS Region Read Access:  %s\n",
		(flmstr & (1 << 17)) ? "enabled" : "disabled");
	printf("  Flash Descriptor Read Access:      %s\n",
		(flmstr & (1 << 16)) ? "enabled" : "disabled");

	printf("  Requester ID:                      0x%04x\n\n",
		flmstr & 0xffff);
}

static void dump_fmba(struct fmba_t *fmba)
{
	printf("Found Master Section\n");
	printf("FLMSTR1:   0x%08x (Host CPU/BIOS)\n", fmba->flmstr1);
	decode_flmstr(fmba->flmstr1);
	printf("FLMSTR2:   0x%08x (Intel ME)\n", fmba->flmstr2);
	decode_flmstr(fmba->flmstr2);
	printf("FLMSTR3:   0x%08x (GbE)\n", fmba->flmstr3);
	decode_flmstr(fmba->flmstr3);
}

static void dump_fmsba(struct fmsba_t *fmsba)
{
	int i;

	printf("Found Processor Strap Section\n");
	for (i = 0; i < 4; i++)
		printf("????:      0x%08x\n", fmsba->data[0]);
}

static void dump_jid(uint32_t jid)
{
	printf("    SPI Componend Device ID 1:          0x%02x\n",
		(jid >> 16) & 0xff);
	printf("    SPI Componend Device ID 0:          0x%02x\n",
		(jid >> 8) & 0xff);
	printf("    SPI Componend Vendor ID:            0x%02x\n",
		jid & 0xff);
}

static void dump_vscc(uint32_t vscc)
{
	printf("    Lower Erase Opcode:                 0x%02x\n",
		vscc >> 24);
	printf("    Lower Write Enable on Write Status: 0x%02x\n",
		vscc & (1 << 20) ? 0x06 : 0x50);
	printf("    Lower Write Status Required:        %s\n",
		vscc & (1 << 19) ? "Yes" : "No");
	printf("    Lower Write Granularity:            %d bytes\n",
		vscc & (1 << 18) ? 64 : 1);
	printf("    Lower Block / Sector Erase Size:    ");
	switch ((vscc >> 16) & 0x3) {
	case 0:
		printf("256 Byte\n");
		break;
	case 1:
		printf("4KB\n");
		break;
	case 2:
		printf("8KB\n");
		break;
	case 3:
		printf("64KB\n");
		break;
	}

	printf("    Upper Erase Opcode:                 0x%02x\n",
		(vscc >> 8) & 0xff);
	printf("    Upper Write Enable on Write Status: 0x%02x\n",
		vscc & (1 << 4) ? 0x06 : 0x50);
	printf("    Upper Write Status Required:        %s\n",
		vscc & (1 << 3) ? "Yes" : "No");
	printf("    Upper Write Granularity:            %d bytes\n",
		vscc & (1 << 2) ? 64 : 1);
	printf("    Upper Block / Sector Erase Size:    ");
	switch (vscc & 0x3) {
	case 0:
		printf("256 Byte\n");
		break;
	case 1:
		printf("4KB\n");
		break;
	case 2:
		printf("8KB\n");
		break;
	case 3:
		printf("64KB\n");
		break;
	}
}

static void dump_vtba(struct vtba_t *vtba, int vtl)
{
	int i;
	int num = (vtl >> 1) < 8 ? (vtl >> 1) : 8;

	printf("ME VSCC table:\n");
	for (i = 0; i < num; i++) {
		printf("  JID%d:  0x%08x\n", i, vtba->entry[i].jid);
		dump_jid(vtba->entry[i].jid);
		printf("  VSCC%d: 0x%08x\n", i, vtba->entry[i].vscc);
		dump_vscc(vtba->entry[i].vscc);
	}
	printf("\n");
}

static void dump_oem(uint8_t *oem)
{
	int i, j;
	printf("OEM Section:\n");
	for (i = 0; i < 4; i++) {
		printf("%02x:", i << 4);
		for (j = 0; j < 16; j++)
			printf(" %02x", oem[(i<<4)+j]);
		printf("\n");
	}
	printf("\n");
}

static int dump_fd(char *image, int size)
{
	struct fdbar_t *fdb = find_fd(image, size);

	if (!fdb)
		return -1;

	printf("FLMAP0:    0x%08x\n", fdb->flmap0);
	printf("  NR:      %d\n", (fdb->flmap0 >> 24) & 7);
	printf("  FRBA:    0x%x\n", ((fdb->flmap0 >> 16) & 0xff) << 4);
	printf("  NC:      %d\n", ((fdb->flmap0 >> 8) & 3) + 1);
	printf("  FCBA:    0x%x\n", ((fdb->flmap0) & 0xff) << 4);

	printf("FLMAP1:    0x%08x\n", fdb->flmap1);
	printf("  ISL:     0x%02x\n", (fdb->flmap1 >> 24) & 0xff);
	printf("  FPSBA:   0x%x\n", ((fdb->flmap1 >> 16) & 0xff) << 4);
	printf("  NM:      %d\n", (fdb->flmap1 >> 8) & 3);
	printf("  FMBA:    0x%x\n", ((fdb->flmap1) & 0xff) << 4);

	printf("FLMAP2:    0x%08x\n", fdb->flmap2);
	printf("  PSL:     0x%04x\n", (fdb->flmap2 >> 8) & 0xffff);
	printf("  FMSBA:   0x%x\n", ((fdb->flmap2) & 0xff) << 4);

	printf("FLUMAP1:   0x%08x\n", fdb->flumap1);
	printf("  Intel ME VSCC Table Length (VTL):        %d\n",
		(fdb->flumap1 >> 8) & 0xff);
	printf("  Intel ME VSCC Table Base Address (VTBA): 0x%06x\n\n",
		(fdb->flumap1 & 0xff) << 4);
	dump_vtba((struct vtba_t *)
			(image + ((fdb->flumap1 & 0xff) << 4)),
			(fdb->flumap1 >> 8) & 0xff);
	dump_oem((uint8_t *)image + 0xf00);
	dump_frba((struct frba_t *)
			(image + (((fdb->flmap0 >> 16) & 0xff) << 4)));
	dump_fcba((struct fcba_t *) (image + (((fdb->flmap0) & 0xff) << 4)));
	dump_fpsba((struct fpsba_t *)
			(image + (((fdb->flmap1 >> 16) & 0xff) << 4)));
	dump_fmba((struct fmba_t *) (image + (((fdb->flmap1) & 0xff) << 4)));
	dump_fmsba((struct fmsba_t *) (image + (((fdb->flmap2) & 0xff) << 4)));

	return 0;
}

static int write_regions(char *image, int size)
{
	struct fdbar_t *fdb;
	struct frba_t *frba;
	int ret = 0;
	int i;

	fdb =  find_fd(image, size);
	if (!fdb)
		return -1;

	frba = (struct frba_t *)(image + (((fdb->flmap0 >> 16) & 0xff) << 4));

	for (i = 0; i < MAX_REGIONS; i++) {
		struct region_t region;
		int region_fd;

		ret = get_region(frba, i, &region);
		if (ret)
			return ret;
		dump_region(i, frba);
		if (region.size == 0)
			continue;
		region_fd = open(region_filename(i),
				 O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR |
				 S_IWUSR | S_IRGRP | S_IROTH);
		if (write(region_fd, image + region.base, region.size) !=
				region.size) {
			perror("Error while writing");
			ret = -1;
		}
		close(region_fd);
	}

	return ret;
}

static int write_image(char *filename, char *image, int size)
{
	int new_fd;

	debug("Writing new image to %s\n", filename);

	new_fd = open(filename,
			 O_WRONLY | O_CREAT | O_TRUNC,
			 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (write(new_fd, image, size) != size) {
		perror("Error while writing");
		return -1;
	}
	close(new_fd);

	return 0;
}

static void set_spi_frequency(char *image, int size, enum spi_frequency freq)
{
	struct fdbar_t *fdb = find_fd(image, size);
	struct fcba_t *fcba = (struct fcba_t *) (image + (((fdb->flmap0) & 0xff) << 4));

	/* clear bits 21-29 */
	fcba->flcomp &= ~0x3fe00000;
	/* Read ID and Read Status Clock Frequency */
	fcba->flcomp |= freq << 27;
	/* Write and Erase Clock Frequency */
	fcba->flcomp |= freq << 24;
	/* Fast Read Clock Frequency */
	fcba->flcomp |= freq << 21;
}

static void set_em100_mode(char *image, int size)
{
	struct fdbar_t *fdb = find_fd(image, size);
	struct fcba_t *fcba = (struct fcba_t *) (image + (((fdb->flmap0) & 0xff) << 4));

	fcba->flcomp &= ~(1 << 30);
	set_spi_frequency(image, size, SPI_FREQUENCY_20MHZ);
}

static void lock_descriptor(char *image, int size)
{
	struct fdbar_t *fdb = find_fd(image, size);
	struct fmba_t *fmba = (struct fmba_t *) (image + (((fdb->flmap1) & 0xff) << 4));
	/*
	 * TODO: Dynamically take Platform Data Region and GbE Region
	 * into account.
	 */
	fmba->flmstr1 = 0x0a0b0000;
	fmba->flmstr2 = 0x0c0d0000;
	fmba->flmstr3 = 0x08080118;
}

static void unlock_descriptor(char *image, int size)
{
	struct fdbar_t *fdb = find_fd(image, size);
	struct fmba_t *fmba = (struct fmba_t *) (image + (((fdb->flmap1) & 0xff) << 4));
	fmba->flmstr1 = 0xffff0000;
	fmba->flmstr2 = 0xffff0000;
	fmba->flmstr3 = 0x08080118;
}

int open_for_read(const char *fname, int *sizep)
{
	int fd = open(fname, O_RDONLY);
	struct stat buf;

	if (fd == -1) {
		perror("Could not open file");
		return -1;
	}
	if (fstat(fd, &buf) == -1) {
		perror("Could not stat file");
		return -1;
	}
	*sizep = buf.st_size;
	debug("File %s is %d bytes\n", fname, *sizep);

	return fd;
}


int inject_region(char *image, int size, int region_type, char *region_fname)
{
	int ret;
	struct fdbar_t *fdb = find_fd(image, size);
	int region_fd;
	int region_size;
	
	if (!fdb)
		exit(EXIT_FAILURE);
	struct frba_t *frba =
	    (struct frba_t *) (image + (((fdb->flmap0 >> 16) & 0xff) << 4));

	struct region_t region;

	ret = get_region(frba, region_type, &region);
	if (ret)
		return -1;
	if (region.size <= 0xfff) {
		fprintf(stderr, "Region %s is disabled in target. Not injecting.\n",
				region_name(region_type));
		return -1;
	}

	region_fd = open_for_read(region_fname, &region_size);
	if (region_fd < 0)
		return region_fd;

	if ( (region_size > region.size) || ((region_type != 1) &&
		(region_size > region.size))) {
		fprintf(stderr, "Region %s is %d(0x%x) bytes. File is %d(0x%x)"
				" bytes. Not injecting.\n",
				region_name(region_type), region.size,
				region.size, region_size, region_size);
		return -1;
	}

	int offset = 0;
	if ((region_type == 1) && (region_size < region.size)) {
		fprintf(stderr, "Region %s is %d(0x%x) bytes. File is %d(0x%x)"
				" bytes. Padding before injecting.\n",
				region_name(region_type), region.size,
				region.size, region_size, region_size);
		offset = region.size - region_size;
		memset(image + region.base, 0xff, offset);
	}

	if (size < region.base + offset + region_size) {
		fprintf(stderr, "Output file is too small. (%d < %d)\n",
			size, region.base + offset + region_size);
		return -1;
	}

	if (read(region_fd, image + region.base + offset, region_size)
							!= region_size) {
		perror("Could not read file");
		return -1;
	}

	close(region_fd);

	debug("Adding %s as the %s section\n", region_fname,
	      region_name(region_type));

	return 0;
}

static int write_data(char *image, int size, unsigned int addr,
		      const char *write_fname)
{
	int write_fd, write_size;
	int offset;

	write_fd = open_for_read(write_fname, &write_size);
	if (write_fd < 0)
		return write_fd;

	offset = addr + size;
	debug("Writing %s to offset %#x\n", write_fname, offset);

	if (offset < 0 || offset + write_size > size) {
		fprintf(stderr, "Output file is too small. (%d < %d)\n",
			size, offset + write_size);
		return -1;
	}

	if (read(write_fd, image + offset, write_size) != write_size) {
		perror("Could not read file");
		return -1;
	}

	close(write_fd);

	return 0;
}

static void print_version(void)
{
	printf("ifdtool v%s -- ", IFDTOOL_VERSION);
	printf("Copyright (C) 2011 Google Inc.\n\n");
	printf
	    ("This program is free software: you can redistribute it and/or modify\n"
	     "it under the terms of the GNU General Public License as published by\n"
	     "the Free Software Foundation, version 2 of the License.\n\n"
	     "This program is distributed in the hope that it will be useful,\n"
	     "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	     "GNU General Public License for more details.\n\n"
	     "You should have received a copy of the GNU General Public License\n"
	     "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\n");
}

static void print_usage(const char *name)
{
	printf("usage: %s [-vhdix?] <filename> [<outfile>]\n", name);
	printf("\n"
	       "   -d | --dump:                      dump intel firmware descriptor\n"
	       "   -x | --extract:                   extract intel fd modules\n"
	       "   -i | --inject <region>:<module>   inject file <module> into region <region>\n"
	       "   -w | --write <addr>:<file>        write file to appear at memory address <addr>\n"
	       "   -s | --spifreq <20|33|50>         set the SPI frequency\n"
	       "   -e | --em100                      set SPI frequency to 20MHz and disable\n"
	       "                                     Dual Output Fast Read Support\n"
	       "   -l | --lock                       Lock firmware descriptor and ME region\n"
	       "   -u | --unlock                     Unlock firmware descriptor and ME region\n"
	       "   -r | --romsize                    Specify ROM size\n"
	       "   -D | --write-descriptor <file>    Write descriptor at base\n"
	       "   -c | --create                     Create a new empty image\n"
	       "   -v | --version:                   print the version\n"
	       "   -h | --help:                      print this help\n\n"
	       "<region> is one of Descriptor, BIOS, ME, GbE, Platform\n"
	       "\n");
}

static int get_two_words(const char *str, char **firstp, char **secondp)
{
	const char *p;

	p = strchr(str, ':');
	if (!p)
		return -1;
	*firstp = strdup(str);
	(*firstp)[p - str] = '\0';
	*secondp = strdup(p + 1);

	return 0;
}

int main(int argc, char *argv[])
{
	int opt, option_index = 0;
	int mode_dump = 0, mode_extract = 0, mode_inject = 0, mode_spifreq = 0;
	int mode_em100 = 0, mode_locked = 0, mode_unlocked = 0, mode_write = 0;
	int mode_write_descriptor = 0, create = 0;
	char *region_type_string = NULL, *src_fname = NULL;
	char *addr_str = NULL;
	int region_type = -1, inputfreq = 0;
	enum spi_frequency spifreq = SPI_FREQUENCY_20MHZ;
	unsigned int addr = 0;
	int rom_size = -1;
	bool write_it;
	int ret;

	static struct option long_options[] = {
		{"create", 0, NULL, 'c'},
		{"dump", 0, NULL, 'd'},
		{"descriptor", 1, NULL, 'D'},
		{"em100", 0, NULL, 'e'},
		{"extract", 0, NULL, 'x'},
		{"inject", 1, NULL, 'i'},
		{"lock", 0, NULL, 'l'},
		{"romsize", 1, NULL, 'r'},
		{"spifreq", 1, NULL, 's'},
		{"unlock", 0, NULL, 'u'},
		{"write", 1, NULL, 'w'},
		{"version", 0, NULL, 'v'},
		{"help", 0, NULL, 'h'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "cdD:ehi:lr:s:uvw:x?",
				  long_options, &option_index)) != EOF) {
		switch (opt) {
		case 'c':
			create = 1;
			break;
		case 'd':
			mode_dump = 1;
			break;
		case 'D':
			mode_write_descriptor = 1;
			src_fname = optarg;
			break;
		case 'e':
			mode_em100 = 1;
			break;
		case 'i':
			if (get_two_words(optarg, &region_type_string,
					  &src_fname)) {
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			// Descriptor, BIOS, ME, GbE, Platform
			// valid type?
			if (!strcasecmp("Descriptor", region_type_string))
				region_type = 0;
			else if (!strcasecmp("BIOS", region_type_string))
				region_type = 1;
			else if (!strcasecmp("ME", region_type_string))
				region_type = 2;
			else if (!strcasecmp("GbE", region_type_string))
				region_type = 3;
			else if (!strcasecmp("Platform", region_type_string))
				region_type = 4;
			if (region_type == -1) {
				fprintf(stderr, "No such region type: '%s'\n\n",
					region_type_string);
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			mode_inject = 1;
			break;
		case 'l':
			mode_locked = 1;
			break;
		case 'r':
			rom_size = strtol(optarg, NULL, 0);
			debug("ROM size %d\n", rom_size);
			break;
		case 's':
			// Parse the requested SPI frequency
			inputfreq = strtol(optarg, NULL, 0);
			switch (inputfreq) {
			case 20:
				spifreq = SPI_FREQUENCY_20MHZ;
				break;
			case 33:
				spifreq = SPI_FREQUENCY_33MHZ;
				break;
			case 50:
				spifreq = SPI_FREQUENCY_50MHZ;
				break;
			default:
				fprintf(stderr, "Invalid SPI Frequency: %d\n",
					inputfreq);
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			mode_spifreq = 1;
			break;
		case 'u':
			mode_unlocked = 1;
			break;
		case 'v':
			print_version();
			exit(EXIT_SUCCESS);
			break;
		case 'w':
			mode_write = 1;
			if (get_two_words(optarg, &addr_str, &src_fname)) {
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			addr = strtol(optarg, NULL, 0);
			break;
		case 'x':
			mode_extract = 1;
			break;
		case 'h':
		case '?':
		default:
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		}
	}

	if (mode_locked == 1 && mode_unlocked == 1) {
		fprintf(stderr, "Locking/Unlocking FD and ME are mutually exclusive\n");
		exit(EXIT_FAILURE);
	}

	if (mode_inject == 1 && mode_write == 1) {
		fprintf(stderr, "Inject/Write are mutually exclusive\n");
		exit(EXIT_FAILURE);
	}

	if ((mode_dump + mode_extract + mode_inject +
		(mode_spifreq | mode_em100 | mode_unlocked |
		 mode_locked)) > 1) {
		fprintf(stderr, "You may not specify more than one mode.\n\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((mode_dump + mode_extract + mode_inject + mode_spifreq +
	     mode_em100 + mode_locked + mode_unlocked + mode_write +
	     mode_write_descriptor) == 0) {
		fprintf(stderr, "You need to specify a mode.\n\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (create && rom_size == -1) {
		fprintf(stderr, "You need to specify a rom size when creating.\n\n");
		exit(EXIT_FAILURE);
	}

	if (optind + 1 != argc) {
		fprintf(stderr, "You need to specify a file.\n\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	char *filename = argv[optind];
	char *outfile = NULL;
	struct stat buf;
	int size = 0;
	int bios_fd;

	if (optind + 2 != argc)
		outfile = argv[optind + 1];

	if (create)
		bios_fd = open(filename, O_WRONLY | O_CREAT, 0666);
	else
		bios_fd = open(filename, outfile ? O_RDONLY : O_RDWR);

	if (bios_fd == -1) {
		perror("Could not open file");
		exit(EXIT_FAILURE);
	}

	if (!create) {
		if (fstat(bios_fd, &buf) == -1) {
			perror("Could not stat file");
			exit(EXIT_FAILURE);
		}
		size = buf.st_size;
	}

	debug("File %s is %d bytes\n", filename, size);

	if (rom_size == -1)
		rom_size = size;

	char *image = malloc(rom_size);
	if (!image) {
		printf("Out of memory.\n");
		exit(EXIT_FAILURE);
	}

	memset(image, '\xff', rom_size);
	if (!create && read(bios_fd, image, size) != size) {
		perror("Could not read file");
		exit(EXIT_FAILURE);
	}
	if (size != rom_size) {
		debug("ROM size changed to %d bytes\n", rom_size);
		size = rom_size;
	}

	write_it = true;
	ret = 0;
	if (mode_dump) {
		ret = dump_fd(image, size);
		write_it = false;
	}

	if (mode_extract) {
		ret = write_regions(image, size);
		write_it = false;
	}

	if (mode_write_descriptor)
		ret = write_data(image, size, -size, src_fname);

	if (mode_inject)
		ret = inject_region(image, size, region_type, src_fname);

	if (mode_write)
		ret = write_data(image, size, addr, src_fname);

	if (mode_spifreq)
		set_spi_frequency(image, size, spifreq);

	if (mode_em100)
		set_em100_mode(image, size);

	if (mode_locked)
		lock_descriptor(image, size);

	if (mode_unlocked)
		unlock_descriptor(image, size);

	if (write_it) {
		if (outfile) {
			ret = write_image(outfile, image, size);
		} else {
			if (lseek(bios_fd, 0, SEEK_SET)) {
				perror("Error while seeking");
				ret = -1;
			}
			if (write(bios_fd, image, size) != size) {
				perror("Error while writing");
				ret = -1;
			}
		}
	}

	free(image);
	close(bios_fd);

	return ret ? 1 : 0;
}
