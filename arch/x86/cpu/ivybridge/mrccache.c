/*
 * From Coreboot src/southbridge/intel/bd82x6x/me_status.c
 *
 * Copyright (C) 2012 Google Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/ip_checksum.h>
#include <asm/arch/sandybridge.h>

/* convert a pointer to flash area into the offset inside the flash */
static inline u32 to_flash_offset(void *p) {
	return ((u32)p + CONFIG_ROM_SIZE);
}

static struct mrc_data_container *next_mrc_block(
	struct mrc_data_container *mrc_cache)
{
	/* MRC data blocks are aligned within the region */
	u32 mrc_size = sizeof(*mrc_cache) + mrc_cache->mrc_data_size;
	if (mrc_size & (MRC_DATA_ALIGN - 1UL)) {
		mrc_size &= ~(MRC_DATA_ALIGN - 1UL);
		mrc_size += MRC_DATA_ALIGN;
	}

	u8 *region_ptr = (u8*)mrc_cache;
	region_ptr += mrc_size;
	return (struct mrc_data_container *)region_ptr;
}

static int is_mrc_cache(struct mrc_data_container *mrc_cache)
{
	return (!!mrc_cache) && (mrc_cache->mrc_signature == MRC_DATA_SIGNATURE);
}

/* Right now, the offsets for the MRC cache area are hard-coded in the
 * northbridge Kconfig if CONFIG_CHROMEOS is not set. In order to make
 * this more flexible, there are two of options:
 *  - Have each mainboard Kconfig supply a hard-coded offset
 *  - Use CBFS
 */
static u32 get_mrc_cache_region(struct mrc_data_container **mrc_region_ptr)
{
	u32 region_size;
#if CONFIG_CHROMEOS
	region_size =  find_fmap_entry("RW_MRC_CACHE", (void **)mrc_region_ptr);
#else
	region_size = CONFIG_MRC_CACHE_SIZE;
	*mrc_region_ptr = (struct mrc_data_container *)
		(CONFIG_MRC_CACHE_BASE + CONFIG_MRC_CACHE_LOCATION);
#endif

	return region_size;
}

/*
 * Find the largest index block in the MRC cache. Return NULL if non is
 * found.
 */
static struct mrc_data_container *find_current_mrc_cache_local
	(struct mrc_data_container *mrc_cache, u32 region_size)
{
	u32 region_end;
	u32 entry_id = 0;
	struct mrc_data_container *mrc_next = mrc_cache;

	region_end = (u32) mrc_cache + region_size;

	/* Search for the last filled entry in the region */
	while (is_mrc_cache(mrc_next)) {
		entry_id++;
		mrc_cache = mrc_next;
		mrc_next = next_mrc_block(mrc_next);
		if ((u32)mrc_next >= region_end) {
			/* Stay in the MRC data region */
			break;
		}
	}

	if (entry_id == 0) {
		debug("%s: No valid MRC cache found.\n", __func__);
		return NULL;
	}

	/* Verify checksum */
	if (mrc_cache->mrc_checksum !=
	    compute_ip_checksum(mrc_cache->mrc_data,
				mrc_cache->mrc_data_size)) {
		printf("%s: MRC cache checksum mismatch\n", __func__);
		return NULL;
	}

	debug("%s: picked entry %u from cache block\n", __func__,
	      entry_id - 1);

	return mrc_cache;
}

/* SPI code needs malloc/free.
 * Also unknown if writing flash from XIP-flash code is a good idea
 */
#if !defined(__PRE_RAM__)
/* find the first empty block in the MRC cache area.
 * If there's none, return NULL.
 *
 * @mrc_cache_base - base address of the MRC cache area
 * @mrc_cache - current entry (for which we need to find next)
 * @region_size - total size of the MRC cache area
 */

/* TODO */
#if 0
static struct mrc_data_container *find_next_mrc_cache
		(struct mrc_data_container *mrc_cache_base,
		 struct mrc_data_container *mrc_cache,
		 u32 region_size)
{
	u32 region_end = (u32) mrc_cache_base + region_size;

	mrc_cache = next_mrc_block(mrc_cache);
	if ((u32)mrc_cache >= region_end) {
		/* Crossed the boundary */
		mrc_cache = NULL;
		debug("%s: no available entries found\n",
		       __func__);
	} else {
		debug("%s: picked next entry from cache block at %p\n",
		       __func__, mrc_cache);
	}

	return mrc_cache;
}

static void update_mrc_cache(void *unused)
{
	debug("Updating MRC cache data.\n");
	struct mrc_data_container *current = cbmem_find(CBMEM_ID_MRCDATA);
	struct mrc_data_container *cache, *cache_base;
	u32 cache_size;

	if (!current) {
		printk(BIOS_ERR, "No MRC cache in cbmem. Can't update flash.\n");
		return;
	}
	if (current->mrc_data_size == -1) {
		printk(BIOS_ERR, "MRC cache data in cbmem invalid.\n");
		return;
	}

	cache_size = get_mrc_cache_region(&cache_base);
	if (cache_base == NULL) {
		printk(BIOS_ERR, "%s: could not find MRC cache area\n",
		       __func__);
		return;
	}

	/*
	 * we need to:
	 */
	//  0. compare MRC data to last mrc-cache block (exit if same)
	cache = find_current_mrc_cache_local(cache_base, cache_size);

	if (cache && (cache->mrc_data_size == current->mrc_data_size) &&
			(memcmp(cache, current, cache->mrc_data_size) == 0)) {
		printk(BIOS_DEBUG,
			"MRC data in flash is up to date. No update.\n");
		return;
	}

	//  1. use spi_flash_probe() to find the flash, then
	spi_init();
	struct spi_flash *flash = spi_flash_probe(0, 0);
	if (!flash) {
		debug("Could not find SPI device\n");
		return;
	}

	//  2. look up the first unused block
	if (cache)
		cache = find_next_mrc_cache(cache_base, cache, cache_size);

	/*
	 * 3. if no such place exists, erase entire mrc-cache range & use
	 * block 0. First time around the erase is not needed, but this is a
	 * small overhead for simpler code.
	 */
	if (!cache) {
		printk(BIOS_DEBUG,
		       "Need to erase the MRC cache region of %d bytes at %p\n",
		       cache_size, cache_base);

		flash->erase(flash, to_flash_offset(cache_base), cache_size);

		/* we will start at the beginning again */
		cache = cache_base;
	}
	//  4. write mrc data with flash->write()
	debug("Finally: write MRC cache update to flash at %p\n",
	       cache);
	flash->write(flash, to_flash_offset(cache),
		     current->mrc_data_size + sizeof(*current), current);
}

BOOT_STATE_INIT_ENTRIES(mrc_cache_update) = {
	BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY,
	                      update_mrc_cache, NULL),
};
#endif
#endif

struct mrc_data_container *find_current_mrc_cache(void)
{
	struct mrc_data_container *cache_base;
	u32 cache_size;

	cache_size = get_mrc_cache_region(&cache_base);
	if (cache_base == NULL) {
		printf("%s: could not find MRC cache area\n", __func__);
		return NULL;
	}

	/*
	 * we need to:
	 */
	//  0. compare MRC data to last mrc-cache block (exit if same)
	return find_current_mrc_cache_local(cache_base, cache_size);
}

