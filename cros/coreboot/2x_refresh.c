/*
 * Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/*
 * Ivybridge: Enable "Force 2x Refresh" mode in memory controller
 *
 * - enable Force 2x Refresh bit in TC_RFP for channel 0 and 1
 * - find current saved MRC training data in RW_MRC_CACHE region
 * - make a new copy of the training data to set bit 16 in TC_RFP
 *   register for channel 0 and 1
 * - update the checksums in the new copy of MRC training data
 * - write out the new training data to a new slot in the
 *   RW_MRC_CACHE region
 * - apply Protected Range Register to cover RW_MRC_CACHE region
 *   if SPI flash is write protected
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch-coreboot/ipchecksum.h>
#include <crc.h>
#include <malloc.h>
#include <spi_flash.h>
#include <cros/2x_refresh.h>
#include <cros/common.h>
#include <cros/firmware_storage.h>
#include <cros/vboot_flag.h>

/* MRC Saved Data settings - RW_MRC_CACHE region */
#define MRC_SAVED_DATA_BASE		0xffbe0000
#define MRC_SAVED_DATA_OFFSET		0x3e0000
#define MRC_SAVED_DATA_SIZE		0x10000

/* MRC Saved Data structure */
#define MRC_DATA_SIGNATURE		(('M'<<0)|('R'<<8)|('C'<<16)|('D'<<24))
#define MRC_DATA_ALIGN			0x1000

/* Checksum offsets in MRC data buffer */
#define MRC_DATA_OFFSET_BACK_IPCSUM	8
#define MRC_DATA_OFFSET_BACK_CRC32	12

/* TC_RFP register offsets in MRC data buffer */
#define MRC_DATA_OFFSET_TC_RFP_C0	276
#define MRC_DATA_OFFSET_TC_RFP_C1	606

/* TC_RFP register offsets in memory controller */
#define MCHBAR_BASE			0xfed10000
#define MCHBAR_REG_TC_RFP_C0		0x4294
#define MCHBAR_REG_TC_RFP_C1		0x4694

/* SPI registers */
#define SPIBAR_BASE			(MCHBAR_BASE + 0xF800)
#define SPIBAR_PR0			0x74
#define SPIBAR_PRR_SHIFT		12
#define SPIBAR_PRR_MASK			0x1fff
#define SPIBAR_PRR_BASE_SHIFT		0
#define SPIBAR_PRR_LIMIT_SHIFT		16
#define SPIBAR_PRR_WPE			(1 << 31)

/* SPI chip details */
#define LINK_SPI_RDSR			0x05
#define LINK_SPI_SR1_SRP0		0x80

/* TC_RFP bit to force 2x refresh mode */
#define TC_RFP_FORCE_2X_REFRESH		(1 << 16)

/* MRC Saved Data wrapper */
struct mrc_saved_data {
	uint32_t signature;
	uint32_t size;
	uint32_t checksum;
	uint32_t reserved;
	uint8_t  data[0];
} __packed;

struct mrc_data_region {
	void *base;
	uint32_t offset;
	uint32_t size;
};

static int mrc_cache_get_region(struct mrc_data_region *region)
{
	region->base = (void *)MRC_SAVED_DATA_BASE;
	region->offset = MRC_SAVED_DATA_OFFSET;
	region->size = MRC_SAVED_DATA_SIZE;
	return 0;
}

static int mrc_cache_in_region(const struct mrc_data_region *region,
			       const struct mrc_saved_data *cache)
{
	uintptr_t region_end;
	uintptr_t cache_end;

	if ((uintptr_t)cache < (uintptr_t)region->base)
		return 0;

	region_end = (uintptr_t)region->base;
	region_end += region->size;

	if ((uintptr_t)cache >= region_end)
		return 0;

	if ((sizeof(*cache) + (uintptr_t)cache) >= region_end)
		return 0;

	cache_end = (uintptr_t)cache;
	cache_end += cache->size + sizeof(*cache);

	if (cache_end > region_end)
		return 0;

	return 1;
}

static int mrc_cache_valid(const struct mrc_data_region *region,
			   const struct mrc_saved_data *cache)
{
	uint32_t checksum;

	if (cache->signature != MRC_DATA_SIGNATURE)
		return 0;

	if (cache->size > region->size)
		return 0;

	if (cache->reserved != 0)
		return 0;

	checksum = ipchksum((uint16_t *)&cache->data[0], cache->size);

	if (cache->checksum != checksum)
		return 0;

	return 1;
}

static const struct mrc_saved_data *
next_cache_block(const struct mrc_saved_data *cache)
{
	uintptr_t next = (uintptr_t)cache;

	next += ALIGN(cache->size + sizeof(*cache), MRC_DATA_ALIGN);

	return (const struct mrc_saved_data *)next;
}

static const struct mrc_saved_data *
mrc_cache_next_slot(const struct mrc_data_region *region,
		    const struct mrc_saved_data *current_slot)
{
	const struct mrc_saved_data *next_slot;

	if (current_slot == NULL)
		next_slot = region->base;
	else
		next_slot = next_cache_block(current_slot);

	return next_slot;
}

/* Locate the most recently saved MRC data. */
static int mrc_cache_get_current(const struct mrc_data_region *region,
				 const struct mrc_saved_data **cache)
{
	const struct mrc_saved_data *msd;
	const struct mrc_saved_data *verified_cache;
	int slot = 0;

	msd = region->base;

	verified_cache = NULL;

	while (mrc_cache_in_region(region, msd) &&
	       mrc_cache_valid(region, msd)) {
		verified_cache = msd;
		msd = next_cache_block(msd);
		slot++;
	}

	if (verified_cache == NULL)
		return -1;

	*cache = verified_cache;
	printf("MRC cache slot %d @ %p\n", slot-1, verified_cache);

	return 0;
}

static int nvm_is_erased(const void *start, int len)
{
	const uint8_t *cur = start;

	for (; len > 0; cur++, len--) {
		if (*cur != 0xff)
			return 0;
	}
	return 1;
}

static int mrc_slot_valid(const struct mrc_data_region *region,
			  const struct mrc_saved_data *slot,
			  const struct mrc_saved_data *to_save)
{
	uintptr_t region_begin;
	uintptr_t region_end;
	uintptr_t slot_end;
	uintptr_t slot_begin;
	uint32_t size;

	region_begin = (uintptr_t)region->base;
	region_end = region_begin + region->size;
	slot_begin = (uintptr_t)slot;
	size = to_save->size + sizeof(*to_save);
	slot_end = slot_begin + size;

	if (slot_begin < region_begin || slot_begin >= region_end)
		return 0;

	if (size > region->size)
		return 0;

	if (slot_end > region_end || slot_end < region_begin)
		return 0;

	if (!nvm_is_erased(slot, size))
		return 0;

	return 1;
}

static int fixup_mrc_saved_data(struct mrc_saved_data *msd)
{
	uint32_t checksum, csum;
	uint32_t c0, c1;
	int need_update = 0;

	/* Extract TC_RFP for C0/C1 */
	memcpy(&c0, &msd->data[MRC_DATA_OFFSET_TC_RFP_C0], sizeof(c0));
	memcpy(&c1, &msd->data[MRC_DATA_OFFSET_TC_RFP_C1], sizeof(c1));

	if (!(c0 & TC_RFP_FORCE_2X_REFRESH)) {
		need_update = 1;
		c0 |= TC_RFP_FORCE_2X_REFRESH;
		memcpy(&msd->data[MRC_DATA_OFFSET_TC_RFP_C0], &c0, sizeof(c0));
	}

	if (!(c1 & TC_RFP_FORCE_2X_REFRESH)) {
		need_update = 1;
		c1 |= TC_RFP_FORCE_2X_REFRESH;
		memcpy(&msd->data[MRC_DATA_OFFSET_TC_RFP_C1], &c1, sizeof(c1));
	}

	/* Return now if no update needed */
	if (!need_update)
		return 1;

	/* Calculate inner CRC32 */
	memcpy(&checksum, &msd->data[msd->size - MRC_DATA_OFFSET_BACK_CRC32],
	       sizeof(checksum));
	csum = crc32(0, msd->data, msd->size - MRC_DATA_OFFSET_BACK_CRC32);

	/* CRC32 did not change, no update needed */
	if (checksum == csum)
		return 1;

	/* Update CRC in saved data region */
	memcpy(&msd->data[msd->size - MRC_DATA_OFFSET_BACK_CRC32], &csum,
	       sizeof(csum));

	/* Calculate outer IP checksum */
	csum = ipchksum((uint16_t *)msd->data,
			msd->size - MRC_DATA_OFFSET_BACK_IPCSUM);
	msd->checksum = csum;

	return 0;
}

/* Protect RW_MRC_CACHE region with a Protected Range Register */
static int protect_mrc_cache(struct mrc_data_region *region,
			     struct spi_flash *flash)
{
	struct vboot_flag_details wpsw;
	uint32_t prr, begin, end;
	uint8_t sr1;
	int wp_gpio, wp_spi;

	/* Read WP GPIO from VBOOT flags */
	if (vboot_flag_fetch(VBOOT_FLAG_WRITE_PROTECT, &wpsw)) {
		printf("Failed to fetch WP GPIO\n");
		return -1;
	}
	wp_gpio = wpsw.active_high ? wpsw.value : !wpsw.value;

	/* Read Status Register 1 */
	if (spi_flash_cmd(flash->spi, LINK_SPI_RDSR, &sr1, 1) < 0) {
		printf("Failed to read SPI status register 1\n");
		return -1;
	}
	wp_spi = !!(sr1 & LINK_SPI_SR1_SRP0);

	printf("SPI flash protection: WPSW=%d SRP0=%d\n", wp_gpio, wp_spi);

	/* Do not apply PRR if flash is not write protected */
	if (!wp_gpio || !wp_spi) {
		printf("NOT enabling PRR for RW_MRC_CACHE region\n");
		return 1;
	}

	/* RW_MRC_CACHE region */
	begin = region->offset;
	end = region->offset + region->size - 1;

	/* Set Protected Range Register */
	prr = readl(SPIBAR_BASE + SPIBAR_PR0);
	prr = ((end >> SPIBAR_PRR_SHIFT) & SPIBAR_PRR_MASK);
	prr <<= SPIBAR_PRR_LIMIT_SHIFT;
	prr |= ((begin >> SPIBAR_PRR_SHIFT) & SPIBAR_PRR_MASK);
	prr |= SPIBAR_PRR_WPE;
	writel(prr, SPIBAR_BASE + SPIBAR_PR0);

	printf("Enabled Protected Range on RW_MRC_CACHE region\n");

	return 0;
}

/* Enable 2x Refresh in MRC saved data */
static void enable_2x_refresh_mrc_cache(void)
{
	firmware_storage_t file;
	struct spi_flash *flash;
	const struct mrc_saved_data *current_saved;
	const struct mrc_saved_data *next_slot;
	struct mrc_saved_data *msd;
	struct mrc_data_region region;
	uint32_t offset;
	int ret;

	if (mrc_cache_get_region(&region)) {
		printf("Could not obtain MRC cache region\n");
		return;
	}

	/* Find current MRC saved data */
	if (mrc_cache_get_current(&region, &current_saved) < 0) {
		printf("Unable to find MRC saved data\n");
		return;
	}

	/* Prepare SPI flash driver */
	if (firmware_storage_open_spi(&file)) {
		printf("Unable to open firmware storage\n");
		return;
	}
	flash = file.context;

	/* Make a copy of current data */
	msd = malloc(sizeof(*msd) + current_saved->size);
	memcpy(msd, current_saved, sizeof(*msd) + current_saved->size);

	/* Update TC_RFP if needed */
	ret = fixup_mrc_saved_data(msd);
	if (ret == 1) {
		/* No update needed, protect RW_MRC_CACHE region */
		printf("2x Refresh already enabled in RW_MRC_CACHE\n");
		goto done;
	} else if (ret < 0) {
		/* Error */
		printf("Error updating MRC saved data\n");
		goto done;
	}

	/* Make sure updated data is valid */
	if (!mrc_cache_valid(&region, msd)) {
		printf("Updated MRC saved data is invalid\n");
		goto done;
	}

	/* See if the data has actually changed */
	if (current_saved->size == msd->size &&
	    !memcmp(&current_saved->data[0], &msd->data[0],
		    current_saved->size)) {
		printf("MRC saved data already udpated\n");
		goto done;
	}

	/* Find next slot to save in */
	next_slot = mrc_cache_next_slot(&region, current_saved);

	/* Erase the region and start over if the slot is invalid */
	if (!mrc_slot_valid(&region, next_slot, msd)) {
		printf("Slot @ %p is invalid\n", next_slot);
		if (!nvm_is_erased(region.base, region.size)) {
			if (flash->erase(flash, region.offset,
					 region.size) < 0) {
				printf("Failure erasing region\n");
				goto done;
			}
		}
		next_slot = region.base;
	}

	/* Write the new updated MRC saved data */
	offset = (uint32_t)((uint8_t *)next_slot + flash->size);
	if (flash->write(flash, offset, msd->size + sizeof(*msd), msd)) {
		printf("Failure writing MRC cache to %p\n",
		       next_slot);
		goto done;
	}

	printf("2x Refresh enabled in RW_MRC_CACHE at offset 0x%x\n", offset);

done:
	/* Protect RW_MRC_CACHE region */
	protect_mrc_cache(&region, flash);

	/* Clean up */
	spi_flash_free(flash);
	free(msd);
}

/* Update TC_RFP register for specified DRAM channel */
static int fixup_mem_ctrlr_channel(int channel)
{
	uint32_t reg, val;

	reg = MCHBAR_BASE;
	reg += channel ? MCHBAR_REG_TC_RFP_C1 : MCHBAR_REG_TC_RFP_C0;
	val = readl(reg);

	if (!(val & TC_RFP_FORCE_2X_REFRESH)) {
		val |= TC_RFP_FORCE_2X_REFRESH;
		writel(val, reg);
		printf("Updated TC_RFP_C%d @ 0x%08x = 0x%08x\n",
		       channel, reg, val);
		return 1;
	}
	return 0;
}

/* Enable 2x Refresh in memory controller for channel 0 and 1 */
static void enable_2x_refresh_mem_ctrlr(void)
{
	int ret = 0;

	ret |= fixup_mem_ctrlr_channel(0);
	ret |= fixup_mem_ctrlr_channel(1);

	printf("2x Refresh %s enabled in memory controller\n",
	       ret ? "now" : "already");
}

void enable_2x_refresh(void)
{
	/* Check and enable 2x Refresh in memory controller */
	enable_2x_refresh_mem_ctrlr();

	/* Enable 2x Refresh in MRC saved data */
	enable_2x_refresh_mrc_cache();
}
