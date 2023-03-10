// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <string.h>
#include <uuid.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include <dm/devres.h>
#include <dm/root.h>
#include <linux/errno.h>
#include <linux/sizes.h>
#include "arm_ffa_priv.h"

DECLARE_GLOBAL_DATA_PTR;

/* FF-A discovery information */
struct ffa_discovery_info dscvry_info;

/* The function that sets the SMC conduit */
static int ffa_set_smc_conduit(void);

/* Error mapping declarations */

int ffa_to_std_errmap[MAX_NUMBER_FFA_ERR] = {
	[NOT_SUPPORTED] = -EOPNOTSUPP,
	[INVALID_PARAMETERS] = -EINVAL,
	[NO_MEMORY] = -ENOMEM,
	[BUSY] = -EBUSY,
	[INTERRUPTED] = -EINTR,
	[DENIED] = -EACCES,
	[RETRY] = -EAGAIN,
	[ABORTED] = -ECANCELED,
};

struct ffa_abi_errmap err_msg_map[FFA_ERRMAP_COUNT] = {
	[FFA_ID_TO_ERRMAP_ID(FFA_VERSION)] = {
		{
			[NOT_SUPPORTED] =
			"NOT_SUPPORTED: A Firmware Framework implementation does not exist",
		},
	},
	[FFA_ID_TO_ERRMAP_ID(FFA_ID_GET)] = {
		{
			[NOT_SUPPORTED] =
			"NOT_SUPPORTED: This function is not implemented at this FF-A instance",
		},
	},
	[FFA_ID_TO_ERRMAP_ID(FFA_FEATURES)] = {
		{
			[NOT_SUPPORTED] =
			"NOT_SUPPORTED: FFA_RXTX_MAP is not implemented at this FF-A instance",
		},
	},
	[FFA_ID_TO_ERRMAP_ID(FFA_PARTITION_INFO_GET)] = {
		{
			[NOT_SUPPORTED] =
			"NOT_SUPPORTED: This function is not implemented at this FF-A instance",
			[INVALID_PARAMETERS] =
			"INVALID_PARAMETERS: Unrecognized UUID",
			[NO_MEMORY] =
			"NO_MEMORY: Results cannot fit in RX buffer of the caller",
			[BUSY] =
			"BUSY: RX buffer of the caller is not free",
			[DENIED] =
			"DENIED: Callee is not in a state to handle this request",
		},
	},
	[FFA_ID_TO_ERRMAP_ID(FFA_RXTX_UNMAP)] = {
		{
			[NOT_SUPPORTED] =
			"NOT_SUPPORTED: FFA_RXTX_UNMAP is not implemented at this FF-A instance",
			[INVALID_PARAMETERS] =
			"INVALID_PARAMETERS: No buffer pair registered on behalf of the caller",
		},
	},
	[FFA_ID_TO_ERRMAP_ID(FFA_RX_RELEASE)] = {
		{
			[NOT_SUPPORTED] =
			"NOT_SUPPORTED: FFA_RX_RELEASE is not implemented at this FF-A instance",
			[DENIED] =
			"DENIED: Caller did not have ownership of the RX buffer",
		},
	},
	[FFA_ID_TO_ERRMAP_ID(FFA_RXTX_MAP)] = {
		{
			[NOT_SUPPORTED] =
			"NOT_SUPPORTED: This function is not implemented at this FF-A instance",
			[INVALID_PARAMETERS] =
			"INVALID_PARAMETERS: Field(s) in input parameters incorrectly encoded",
			[NO_MEMORY] =
			"NO_MEMORY: Not enough memory",
			[DENIED] =
			"DENIED: Buffer pair already registered",
		},
	},
};

/**
 * ffa_to_std_errno() - convert FF-A error code to standard error code
 * @ffa_errno:	Error code returned by the FF-A ABI
 *
 * This function maps the given FF-A error code as specified
 * by the spec to a u-boot standard error code.
 *
 * Return:
 *
 * The standard error code on success. . Otherwise, failure
 */
int ffa_to_std_errno(int ffa_errno)
{
	int err_idx = -ffa_errno;

	/* Map the FF-A error code to the standard u-boot error code */
	if (err_idx > 0 && err_idx < MAX_NUMBER_FFA_ERR)
		return ffa_to_std_errmap[err_idx];
	return -EINVAL;
}

/**
 * ffa_print_error_log() - print the error log corresponding to the selected FF-A ABI
 * @ffa_id:	FF-A ABI ID
 * @ffa_errno:	Error code returned by the FF-A ABI
 *
 * This function maps the FF-A error code to the error log relevant to the
 * selected FF-A ABI. Then the error log is printed.
 *
 * Return:
 *
 * 0 on success. . Otherwise, failure
 */
int ffa_print_error_log(u32 ffa_id, int ffa_errno)
{
	int err_idx = -ffa_errno, abi_idx = 0;

	/* Map the FF-A error code to the corresponding error log */

	if (err_idx <= 0 || err_idx >= MAX_NUMBER_FFA_ERR)
		return -EINVAL;

	if (ffa_id < FFA_FIRST_ID || ffa_id > FFA_LAST_ID)
		return -EINVAL;

	abi_idx = FFA_ID_TO_ERRMAP_ID(ffa_id);
	if (abi_idx < 0 || abi_idx >= FFA_ERRMAP_COUNT)
		return -EINVAL;

	if (!err_msg_map[abi_idx].err_str[err_idx])
		return -EINVAL;

	log_err("[FFA] %s\n", err_msg_map[abi_idx].err_str[err_idx]);

	return 0;
}

/* Driver core functions */

/**
 * ffa_get_version() - FFA_VERSION handler function
 *
 * This function implements FFA_VERSION FF-A function
 * to get from the secure world the FF-A framework version
 * FFA_VERSION is used to discover the FF-A framework.
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_get_version(void)
{
	u16 major, minor;
	ffa_value_t res = {0};
	int ffa_errno;

	if (!dscvry_info.invoke_ffa_fn)
		return -EINVAL;

	dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_32(FFA_VERSION), .a1 = FFA_VERSION_1_0,
			}, &res);

	ffa_errno = res.a0;
	if (ffa_errno < 0) {
		ffa_print_error_log(FFA_VERSION, ffa_errno);
		return ffa_to_std_errno(ffa_errno);
	}

	major = GET_FFA_MAJOR_VERSION(res.a0);
	minor = GET_FFA_MINOR_VERSION(res.a0);

	log_info("[FFA] FF-A driver %d.%d\nFF-A framework %d.%d\n",
		 FFA_MAJOR_VERSION, FFA_MINOR_VERSION, major, minor);

	if (major == FFA_MAJOR_VERSION && minor >= FFA_MINOR_VERSION) {
		log_info("[FFA] Versions are compatible\n");

		dscvry_info.fwk_version = res.a0;

		return 0;
	}

	log_err("[FFA] versions are incompatible\nExpected: %d.%d , Found: %d.%d\n",
		FFA_MAJOR_VERSION, FFA_MINOR_VERSION, major, minor);

	return -EPROTONOSUPPORT;
}

/**
 * ffa_get_endpoint_id() - FFA_ID_GET handler function
 * @dev: The FF-A bus device
 *
 * This function implements FFA_ID_GET FF-A function
 * to get from the secure world u-boot endpoint ID
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_get_endpoint_id(struct udevice *dev)
{
	ffa_value_t res = {0};
	int ffa_errno;
	struct ffa_priv *priv = dev_get_priv(dev);

	priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_32(FFA_ID_GET),
			}, &res);

	if (res.a0 == FFA_SMC_32(FFA_SUCCESS)) {
		priv->id = GET_SELF_ENDPOINT_ID((u32)res.a2);
		log_info("[FFA] endpoint ID is %u\n", priv->id);

		return 0;
	}

	ffa_errno = res.a2;

	ffa_print_error_log(FFA_ID_GET, ffa_errno);

	return ffa_to_std_errno(ffa_errno);
}

/**
 * ffa_set_rxtx_buffers_pages_cnt() - sets the minimum number of pages in each of the RX/TX buffers
 * @dev: The FF-A bus device
 * @prop_field: properties field obtained from FFA_FEATURES ABI
 *
 * This function sets the minimum number of pages
 *  in each of the RX/TX buffers in the private data structure
 *
 * Return:
 *
 * buf_4k_pages points to the returned number of pages
 * 0 on success. Otherwise, failure
 */
static int ffa_set_rxtx_buffers_pages_cnt(struct udevice *dev, u32 prop_field)
{
	struct ffa_priv *priv = dev_get_priv(dev);

	switch (prop_field) {
	case RXTX_4K:
		priv->pair.rxtx_min_pages = 1;
		break;
	case RXTX_16K:
		priv->pair.rxtx_min_pages = 4;
		break;
	case RXTX_64K:
		priv->pair.rxtx_min_pages = 16;
		break;
	default:
		log_err("[FFA] RX/TX buffer size not supported\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * ffa_get_rxtx_map_features() - FFA_FEATURES handler function with FFA_RXTX_MAP argument
 * @dev: The FF-A bus device
 *
 * This function implements FFA_FEATURES FF-A function
 * to retrieve the FFA_RXTX_MAP features
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_get_rxtx_map_features(struct udevice *dev)
{
	ffa_value_t res = {0};
	int ffa_errno;
	struct ffa_priv *priv = dev_get_priv(dev);

	priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_32(FFA_FEATURES),
			.a1 = FFA_SMC_64(FFA_RXTX_MAP),
			}, &res);

	if (res.a0 == FFA_SMC_32(FFA_SUCCESS))
		return ffa_set_rxtx_buffers_pages_cnt(dev, res.a2);

	ffa_errno = res.a2;
	ffa_print_error_log(FFA_FEATURES, ffa_errno);

	return ffa_to_std_errno(ffa_errno);
}

/**
 * ffa_free_rxtx_buffers() - frees the RX/TX buffers
 * @dev: The FF-A bus device
 *
 * This  function  frees the RX/TX buffers
 */
static void ffa_free_rxtx_buffers(struct udevice *dev)
{
	struct ffa_priv *priv = dev_get_priv(dev);

	log_info("[FFA] Freeing RX/TX buffers\n");

	if (priv->pair.rxbuf) {
		free(priv->pair.rxbuf);
		priv->pair.rxbuf = NULL;
	}

	if (priv->pair.txbuf) {
		free(priv->pair.txbuf);
		priv->pair.txbuf = NULL;
	}
}

/**
 * ffa_alloc_rxtx_buffers() - allocates the RX/TX buffers
 * @dev: The FF-A bus device
 *
 * This function is used by ffa_map_rxtx_buffers to allocate
 * the RX/TX buffers before mapping them. The allocated memory is physically
 * contiguous since memalign ends up calling malloc which allocates
 * contiguous memory in u-boot.
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_alloc_rxtx_buffers(struct udevice *dev)
{
	u64 bytes;
	struct ffa_priv *priv = dev_get_priv(dev);

	log_info("[FFA] Using %lu 4KB page(s) for RX/TX buffers size\n",
		 priv->pair.rxtx_min_pages);

	bytes = priv->pair.rxtx_min_pages * SZ_4K;

	/*
	 * The alignment of the RX and TX buffers must be equal
	 * to the larger translation granule size
	 * Assumption: Memory allocated with memalign is always physically contiguous
	 */

	priv->pair.rxbuf = memalign(bytes, bytes);
	if (!priv->pair.rxbuf) {
		log_err("[FFA] failure to allocate RX buffer\n");
		return -ENOBUFS;
	}

	log_info("[FFA] RX buffer at virtual address %p\n", priv->pair.rxbuf);

	priv->pair.txbuf = memalign(bytes, bytes);
	if (!priv->pair.txbuf) {
		free(priv->pair.rxbuf);
		priv->pair.rxbuf = NULL;
		log_err("[FFA] failure to allocate the TX buffer\n");
		return -ENOBUFS;
	}

	log_info("[FFA] TX buffer at virtual address %p\n", priv->pair.txbuf);

	/* Make sure the buffers are cleared before use */
	memset(priv->pair.rxbuf, 0, bytes);
	memset(priv->pair.txbuf, 0, bytes);

	return 0;
}

/**
 * ffa_map_rxtx_buffers() - FFA_RXTX_MAP handler function
 * @dev: The FF-A bus device
 *
 * This function implements FFA_RXTX_MAP FF-A function
 * to map the RX/TX buffers
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_map_rxtx_buffers(struct udevice *dev)
{
	int ret;
	ffa_value_t res = {0};
	int ffa_errno;
	struct ffa_priv *priv = dev_get_priv(dev);

	ret = ffa_alloc_rxtx_buffers(dev);
	if (ret)
		return ret;

	/*
	 * we need to pass the physical addresses of the RX/TX buffers
	 * in u-boot physical/virtual mapping is 1:1
	 * no need to convert from virtual to physical
	 */

	priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_64(FFA_RXTX_MAP),
			.a1 = map_to_sysmem(priv->pair.txbuf),
			.a2 = map_to_sysmem(priv->pair.rxbuf),
			.a3 = priv->pair.rxtx_min_pages,
			}, &res);

	if (res.a0 == FFA_SMC_32(FFA_SUCCESS)) {
		log_info("[FFA] RX/TX buffers mapped\n");
		return 0;
	}

	ffa_errno = res.a2;
	ffa_print_error_log(FFA_RXTX_MAP, ffa_errno);

	ffa_free_rxtx_buffers(dev);

	return ffa_to_std_errno(ffa_errno);
}

/**
 * ffa_unmap_rxtx_buffers() - FFA_RXTX_UNMAP handler function
 * @dev: The arm_ffa bus device
 *
 * This function implements FFA_RXTX_UNMAP FF-A function
 * to unmap the RX/TX buffers
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_unmap_rxtx_buffers(struct udevice *dev)
{
	ffa_value_t res = {0};
	int ffa_errno;
	struct ffa_priv *priv = NULL;

	if (!dev)
		return -ENODEV;

	log_info("[FFA] unmapping RX/TX buffers\n");

	priv = dev_get_priv(dev);

	priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_32(FFA_RXTX_UNMAP),
			.a1 = PREP_SELF_ENDPOINT_ID(priv->id),
			}, &res);

	if (res.a0 == FFA_SMC_32(FFA_SUCCESS)) {
		ffa_free_rxtx_buffers(dev);
		return 0;
	}

	ffa_errno = res.a2;
	ffa_print_error_log(FFA_RXTX_UNMAP, ffa_errno);

	return ffa_to_std_errno(ffa_errno);
}

/**
 * ffa_release_rx_buffer() - FFA_RX_RELEASE handler function
 * @dev: The FF-A bus device
 *
 * This function invokes FFA_RX_RELEASE FF-A function
 * to release the ownership of the RX buffer
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_release_rx_buffer(struct udevice *dev)
{
	ffa_value_t res = {0};
	int ffa_errno;
	struct ffa_priv *priv = dev_get_priv(dev);

	priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_32(FFA_RX_RELEASE),
			}, &res);

	if (res.a0 == FFA_SMC_32(FFA_SUCCESS))
		return 0;

	ffa_errno = res.a2;
	ffa_print_error_log(FFA_RX_RELEASE, ffa_errno);

	return ffa_to_std_errno(ffa_errno);
}

/**
 * ffa_uuid_are_identical() - checks whether two given UUIDs are identical
 * @uuid1: first UUID
 * @uuid2: second UUID
 *
 * This function is used by ffa_read_partitions_info to search
 * for a UUID in the partitions descriptors table
 *
 * Return:
 *
 * 1 when UUIDs match. Otherwise, 0
 */
bool ffa_uuid_are_identical(const struct ffa_partition_uuid *uuid1,
			    const struct ffa_partition_uuid *uuid2)
{
	if (!uuid1 || !uuid2)
		return 0;

	return !memcmp(uuid1, uuid2, sizeof(struct ffa_partition_uuid));
}

/**
 * ffa_read_partitions_info() - reads queried partition data
 * @dev: The arm_ffa bus device
 * @count: The number of partitions queried
 * @part_uuid: Pointer to the partition(s) UUID
 *
 * This function reads the partitions information
 * returned by the FFA_PARTITION_INFO_GET and saves it in the private
 * data structure.
 *
 * Return:
 *
 * The private data structure is updated with the partition(s) information
 * 0 is returned on success. Otherwise, failure
 */
static int ffa_read_partitions_info(struct udevice *dev, u32 count,
				    struct ffa_partition_uuid *part_uuid)
{
	struct ffa_priv *priv = dev_get_priv(dev);

	if (!count) {
		log_err("[FFA] no partition detected\n");
		return -ENODATA;
	}

	log_info("[FFA] Reading partitions data from the RX buffer\n");

	if (!part_uuid) {
		/* Querying information of all partitions */
		u64 buf_bytes;
		u64 data_bytes;
		u32 desc_idx;
		struct ffa_partition_info *parts_info;

		data_bytes = count * sizeof(struct ffa_partition_desc);

		buf_bytes = priv->pair.rxtx_min_pages * SZ_4K;

		if (data_bytes > buf_bytes) {
			log_err("[FFA] partitions data size exceeds the RX buffer size:\n");
			log_err("[FFA]     sizes in bytes: data %llu , RX buffer %llu\n",
				data_bytes,
				buf_bytes);

			return -ENOMEM;
		}

		priv->partitions.descs = devm_kmalloc(dev, data_bytes, __GFP_ZERO);
		if (!priv->partitions.descs) {
			log_err("[FFA] cannot  allocate partitions data buffer\n");
			return -ENOMEM;
		}

		parts_info = priv->pair.rxbuf;

		for (desc_idx = 0 ; desc_idx < count ; desc_idx++) {
			priv->partitions.descs[desc_idx].info =
				parts_info[desc_idx];

			log_info("[FFA] Partition ID %x : info cached\n",
				 priv->partitions.descs[desc_idx].info.id);
		}

		priv->partitions.count = count;

		log_info("[FFA] %d partition(s) found and cached\n", count);

	} else {
		u32 rx_desc_idx, cached_desc_idx;
		struct ffa_partition_info *parts_info;
		u8 desc_found;

		parts_info = priv->pair.rxbuf;

		/*
		 * Search for the SP IDs read from the RX buffer
		 * in the already cached SPs.
		 * Update the UUID when ID found.
		 */
		for (rx_desc_idx = 0; rx_desc_idx < count ; rx_desc_idx++) {
			desc_found = 0;

			/* Search the current ID in the cached partitions */
			for (cached_desc_idx = 0;
			     cached_desc_idx < priv->partitions.count;
			     cached_desc_idx++) {
				/* Save the UUID */
				if (priv->partitions.descs[cached_desc_idx].info.id ==
				    parts_info[rx_desc_idx].id) {
					priv->partitions.descs[cached_desc_idx].sp_uuid =
						*part_uuid;

					desc_found = 1;
					break;
				}
			}

			if (!desc_found)
				return -ENODATA;
		}
	}

	return  0;
}

/**
 * ffa_query_partitions_info() - invokes FFA_PARTITION_INFO_GET and saves partitions data
 * @dev: The arm_ffa bus device
 * @part_uuid: Pointer to the partition(s) UUID
 * @pcount: Pointer to the number of partitions variable filled when querying
 *
 * This function executes the FFA_PARTITION_INFO_GET
 * to query the partitions data. Then, it calls ffa_read_partitions_info
 * to save the data in the private data structure.
 *
 * After reading the data the RX buffer is released using ffa_release_rx_buffer
 *
 * Return:
 *
 * When part_uuid is NULL, all partitions data are retrieved from secure world
 * When part_uuid is non NULL, data for partitions matching the given UUID are
 * retrieved and the number of partitions is returned
 * 0 is returned on success. Otherwise, failure
 */
static int ffa_query_partitions_info(struct udevice *dev, struct ffa_partition_uuid *part_uuid,
				     u32 *pcount)
{
	struct ffa_partition_uuid query_uuid = {0};
	ffa_value_t res = {0};
	int ffa_errno;
	struct ffa_priv *priv = dev_get_priv(dev);

	/*
	 * If a UUID is specified. Information for one or more
	 * partitions in the system is queried. Otherwise, information
	 * for all installed partitions is queried
	 */

	if (part_uuid) {
		if (!pcount)
			return -EINVAL;

		query_uuid = *part_uuid;
	} else if (pcount) {
		return -EINVAL;
	}

	priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_32(FFA_PARTITION_INFO_GET),
			.a1 = query_uuid.a1,
			.a2 = query_uuid.a2,
			.a3 = query_uuid.a3,
			.a4 = query_uuid.a4,
			}, &res);

	if (res.a0 == FFA_SMC_32(FFA_SUCCESS)) {
		int ret;

		/*
		 * res.a2 contains the count of partition information descriptors
		 * populated in the RX buffer
		 */
		if (res.a2) {
			ret = ffa_read_partitions_info(dev, (u32)res.a2, part_uuid);
			if (ret) {
				log_err("[FFA] failed reading SP(s) data , err (%d)\n", ret);
				ffa_release_rx_buffer(dev);
				return -EINVAL;
			}
		}

		/* Return the SP count (when querying using a UUID) */
		if (pcount)
			*pcount = (u32)res.a2;

		/*
		 * After calling FFA_PARTITION_INFO_GET the buffer ownership
		 * is assigned to the consumer (u-boot). So, we need to give
		 * the ownership back to the SPM or hypervisor
		 */
		ret = ffa_release_rx_buffer(dev);

		return ret;
	}

	ffa_errno = res.a2;
	ffa_print_error_log(FFA_PARTITION_INFO_GET, ffa_errno);

	return ffa_to_std_errno(ffa_errno);
}

/**
 * ffa_get_partitions_info() - FFA_PARTITION_INFO_GET handler function
 *
 * The passed arguments:
 * Mode 1: When getting from the driver the number of
 *	secure partitions:
 *	@uuid_str: pointer to the UUID string
 *	@sp_count: pointer to the variable that contains the number of partitions
 *			 The variable will be set by the driver
 *	@buffer: NULL
 *
 * Mode 2: When requesting the driver to return the
 *	partitions information:
 *	@dev: The arm_ffa bus device
 *	@uuid_str: pointer to the UUID string
 *	@sp_count: pointer to the variable that contains the number of empty partition descriptors
 *			 The variable will be read by the driver
 *	@buffer: pointer to SPs information buffer
 *		(allocated by the client and contains empty @sp_count descriptors).
 *		The buffer will be filled by the driver
 *
 * This function queries the secure partition data from
 * the private data structure. If not found, it invokes FFA_PARTITION_INFO_GET
 * FF-A function to query the partition information from secure world.
 *
 * A client of the FF-A driver should know the UUID of the service it wants to
 * access. It should use the UUID to request the FF-A driver to provide the
 * partition(s) information of the service. The FF-A driver uses
 * PARTITION_INFO_GET to obtain this information. This is implemented through
 * ffa_get_partitions_info function.
 * A new FFA_PARTITION_INFO_GET call is issued (first one performed through
 * ffa_cache_partitions_info) allowing to retrieve the partition(s) information.
 * They are not saved (already done). We only update the UUID in the cached area.
 * This assumes that partitions data does not change in the secure world.
 * Otherwise u-boot will have an outdated partition data. The benefit of caching
 * the information in the FF-A driver is to accommodate discovery after
 * ExitBootServices().
 *
 * When invoked through a client request, ffa_get_partitions_info should be
 * called twice. First call is to get from the driver the number of secure
 * partitions (SPs) associated to a particular UUID.
 * Then, the caller (client) allocates the buffer to host the SPs data and
 * issues a 2nd call. Then, the driver fills the SPs data in the pre-allocated
 * buffer.
 *
 * To achieve the mechanism described above, ffa_get_partitions_info uses the
 * following functions:
 *		ffa_read_partitions_info
 *		ffa_query_partitions_info
 *
 * Return:
 *
 * @sp_count: When pointing to the number of partitions variable, the number is
 * set by the driver.
 * When pointing to the partitions information buffer size, the buffer will be
 * filled by the driver.
 *
 * On success 0 is returned. Otherwise, failure
 */
static int ffa_get_partitions_info(struct udevice *dev, const char *uuid_str,
				   u32 *sp_count, struct ffa_partition_info *buffer)
{
	/*
	 * fill_data:
	 * 0: return the SP count
	 * 1: fill SP data and return it to the caller
	 */
	bool fill_data = 0;
	u32 desc_idx, client_desc_idx;
	struct ffa_partition_uuid part_uuid = {0};
	u32 sp_found = 0;
	struct ffa_priv *priv = NULL;

	if (!dev)
		return -ENODEV;

	priv = dev_get_priv(dev);

	if (!priv->partitions.count || !priv->partitions.descs) {
		log_err("[FFA] no partition installed\n");
		return -EINVAL;
	}

	if (!uuid_str) {
		log_err("[FFA] no UUID provided\n");
		return -EINVAL;
	}

	if (!sp_count) {
		log_err("[FFA] no size/count provided\n");
		return -EINVAL;
	}

	if (uuid_str_to_le_bin(uuid_str, (unsigned char *)&part_uuid)) {
		log_err("[FFA] invalid UUID\n");
		return -EINVAL;
	}

	if (!buffer) {
		/* Mode 1: getting the number of secure partitions */

		fill_data = 0;

		log_info("[FFA] Preparing for checking partitions count\n");

	} else if (*sp_count) {
		/* Mode 2: retrieving the partitions information */

		fill_data = 1;

		client_desc_idx = 0;

		log_info("[FFA] Preparing for filling partitions info\n");

	} else {
		log_err("[FFA] invalid function arguments provided\n");
		return -EINVAL;
	}

	log_info("[FFA] Searching partitions using the provided UUID\n");

	/* Search in the cached partitions */
	for (desc_idx = 0;
	     desc_idx < priv->partitions.count;
	     desc_idx++) {
		if (ffa_uuid_are_identical(&priv->partitions.descs[desc_idx].sp_uuid,
					   &part_uuid)) {
			log_info("[FFA] Partition ID %x matches the provided UUID\n",
				 priv->partitions.descs[desc_idx].info.id);

			sp_found++;

			if (fill_data) {
				/* Trying to fill the partition info in the input buffer */

				if (client_desc_idx < *sp_count) {
					buffer[client_desc_idx++] =
						priv->partitions.descs[desc_idx].info;
					continue;
				}

				log_err("[FFA] failed to fill client descriptor, buffer full\n");
				return -ENOBUFS;
			}
		}
	}

	if (!sp_found) {
		int ret;

		log_info("[FFA] No partition found. Querying framework ...\n");

		ret = ffa_query_partitions_info(dev, &part_uuid, &sp_found);

		if (ret == 0) {
			if (!fill_data) {
				*sp_count = sp_found;

				log_info("[FFA] Number of partition(s) matching the UUID: %d\n",
					 sp_found);
			} else {
				/*
				 * If SPs data detected, they are already in the private data
				 * structure, retry searching SP data again to return them
				 *  to the caller
				 */
				if (sp_found)
					ret = ffa_get_partitions_info(dev, uuid_str, sp_count,
								      buffer);
				else
					ret = -ENODATA;
			}
		}

		return ret;
	}

	/* Partition(s) found */
	if (!fill_data)
		*sp_count = sp_found;

	return 0;
}

/**
 * ffa_cache_partitions_info() - Queries and saves all secure partitions data
 * @dev: The arm_ffa bus device
 *
 * This function invokes FFA_PARTITION_INFO_GET FF-A
 * function to query from secure world all partitions information.
 *
 * The FFA_PARTITION_INFO_GET call is issued with nil UUID as an argument.
 * All installed partitions information are returned. We cache them in the
 * resident private data structure and we keep the UUID field empty
 * (in FF-A 1.0 UUID is not provided by the partition descriptor)
 *
 * This function is called at the device probing level.
 * ffa_cache_partitions_info uses ffa_query_partitions_info to get the data
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_cache_partitions_info(struct udevice *dev)
{
	return ffa_query_partitions_info(dev, NULL, NULL);
}

/**
 * ffa_msg_send_direct_req() - FFA_MSG_SEND_DIRECT_{REQ,RESP} handler function
 * @dev: The arm_ffa bus device
 * @dst_part_id: destination partition ID
 * @msg: pointer to the message data preallocated by the client (in/out)
 * @is_smc64: select 64-bit or 32-bit FF-A ABI
 *
 * This function implements FFA_MSG_SEND_DIRECT_{REQ,RESP}
 * FF-A functions.
 *
 * FFA_MSG_SEND_DIRECT_REQ is used to send the data to the secure partition.
 * The response from the secure partition is handled by reading the
 * FFA_MSG_SEND_DIRECT_RESP arguments.
 *
 * The maximum size of the data that can be exchanged is 40 bytes which is
 * sizeof(struct ffa_send_direct_data) as defined by the FF-A specification 1.0
 * in the section relevant to FFA_MSG_SEND_DIRECT_{REQ,RESP}
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_msg_send_direct_req(struct udevice *dev, u16 dst_part_id,
				   struct ffa_send_direct_data *msg, bool is_smc64)
{
	ffa_value_t res = {0};
	int ffa_errno;
	u64 req_mode, resp_mode;
	struct ffa_priv *priv = NULL;

	if (!dev)
		return -ENODEV;

	priv = dev_get_priv(dev);

	if (!priv || !priv->dscvry_info.invoke_ffa_fn)
		return -EINVAL;

	/* No partition installed */
	if (!priv->partitions.count || !priv->partitions.descs)
		return -ENODEV;

	if (is_smc64) {
		req_mode = FFA_SMC_64(FFA_MSG_SEND_DIRECT_REQ);
		resp_mode = FFA_SMC_64(FFA_MSG_SEND_DIRECT_RESP);
	} else {
		req_mode = FFA_SMC_32(FFA_MSG_SEND_DIRECT_REQ);
		resp_mode = FFA_SMC_32(FFA_MSG_SEND_DIRECT_RESP);
	}

	priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = req_mode,
			.a1 = PREP_SELF_ENDPOINT_ID(priv->id) |
				PREP_PART_ENDPOINT_ID(dst_part_id),
			.a2 = 0,
			.a3 = msg->data0,
			.a4 = msg->data1,
			.a5 = msg->data2,
			.a6 = msg->data3,
			.a7 = msg->data4,
			}, &res);

	while (res.a0 == FFA_SMC_32(FFA_INTERRUPT))
		priv->dscvry_info.invoke_ffa_fn((ffa_value_t){
			.a0 = FFA_SMC_32(FFA_RUN),
			.a1 = res.a1,
			}, &res);

	if (res.a0 == FFA_SMC_32(FFA_SUCCESS)) {
		/* Message sent with no response */
		return 0;
	}

	if (res.a0 == resp_mode) {
		/* Message sent with response extract the return data */
		msg->data0 = res.a3;
		msg->data1 = res.a4;
		msg->data2 = res.a5;
		msg->data3 = res.a6;
		msg->data4 = res.a7;

		return 0;
	}

	ffa_errno = res.a2;
	return ffa_to_std_errno(ffa_errno);
}

/**
 * ffa_try_discovery() - performs FF-A discovery
 * Tries to discover the FF-A framework. Discovery is performed by
 * querying the FF-A framework version from secure world using the FFA_VERSION ABI.
 * Return:
 *
 * true on success. Otherwise, false.
 */
bool ffa_try_discovery(void)
{
	int ret;

	log_info("[FFA] trying FF-A framework discovery\n");

	ret = ffa_set_smc_conduit();
	if (ret)
		return false;

	ret = ffa_get_version();
	if (ret)
		return false;

	return true;
}

/**
 * __arm_ffa_fn_smc() - SMC wrapper
 * @args: FF-A ABI arguments to be copied to Xn registers
 * @res: FF-A ABI return data to be copied from Xn registers
 *
 * Calls low level SMC assembly function
 */
void __arm_ffa_fn_smc(ffa_value_t args, ffa_value_t *res)
{
	arm_smccc_1_2_smc(&args, res);
}

/**
 * ffa_bus_is_supported() - FF-A discovery callback
 * @invoke_fn: legacy SMC invoke function (not used)
 *
 * This function performs FF-A discovery by calling ffa_try_discovery().
 * Discovery is performed by querying the FF-A framework version from
 * secure world using the FFA_VERSION ABI.
 *
 * The FF-A driver is registered as an SMCCC feature driver. So, features discovery
 * callbacks are called by the PSCI driver (PSCI device is the SMCCC features
 * root device).
 *
 * The FF-A driver supports the SMCCCv1.2 extended input/output registers.
 * So, the legacy SMC invocation is not used.
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static bool ffa_bus_is_supported(void (*invoke_fn)(unsigned long a0, unsigned long a1,
						   unsigned long a2, unsigned long a3,
						   unsigned long a4, unsigned long a5,
						   unsigned long a6, unsigned long a7,
						   struct arm_smccc_res *res))
{
	return ffa_try_discovery();
}

/* Registering the FF-A driver as an SMCCC feature driver */

ARM_SMCCC_FEATURE_DRIVER(arm_ffa) = {
	.driver_name = FFA_DRV_NAME,
	.is_supported = ffa_bus_is_supported,
};

/**
 * ffa_set_smc_conduit() - Set the SMC conduit
 *
 * This function selects the SMC conduit by setting the driver invoke function
 * to SMC assembly function
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_set_smc_conduit(void)
{
	dscvry_info.invoke_ffa_fn = __arm_ffa_fn_smc;

	log_info("[FFA] Conduit is SMC\n");

	return 0;
}

/**
 * ffa_devm_alloc_priv() - allocate FF-A driver private data
 * @dev: The FF-A bus device
 *
 * This function dynamically allocates with devres the private data structure
 * which contains all the FF-A data. Then, register the structure with the DM.
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_devm_alloc_priv(struct udevice *dev)
{
	struct ffa_priv *priv = dev_get_priv(dev);

	if (!priv) {
		priv = devm_kmalloc(dev, sizeof(struct ffa_priv), __GFP_ZERO);
		if (!priv) {
			log_err("[FFA] can not allocate FF-A main data structure\n");
			return -ENOMEM;
		}
		dev_set_priv(dev, priv);
	}

	return 0;
}

/**
 * ffa_probe() - The driver probe function
 * @dev:	the FF-A bus device (arm_ffa)
 *
 * Probing is triggered on demand by clients searching for the uclass.
 * At probe level the following actions are done:
 *	- allocating private data (priv) with devres
 *	- updating priv with discovery information
 *	- querying from secure world the u-boot endpoint ID
 *	- querying from secure world the supported features of FFA_RXTX_MAP
 *	- mapping the RX/TX buffers
 *	- querying from secure world all the partitions information
 *
 * All data queried from secure world is saved in the private data structure (priv).
 *
 * Return:
 *
 * 0 on success. Otherwise, failure
 */
static int ffa_probe(struct udevice *dev)
{
	int ret;
	struct ffa_priv *priv = NULL;

	ret = ffa_devm_alloc_priv(dev);
	if (ret)
		return ret;

	/* The data is dynamically allocated, managed by devres */
	priv = dev_get_priv(dev);

	priv->dscvry_info = dscvry_info;

	ret = ffa_get_endpoint_id(dev);
	if (ret)
		return ret;

	ret = ffa_get_rxtx_map_features(dev);
	if (ret)
		return ret;

	ret = ffa_map_rxtx_buffers(dev);
	if (ret)
		return ret;

	ret = ffa_cache_partitions_info(dev);
	if (ret) {
		ffa_unmap_rxtx_buffers(dev);
		return ret;
	}

	return 0;
}

/**
 * ffa_remove() - The driver remove function
 * @dev:	the arm_ffa device
 * Making sure the RX/TX buffers are unmapped and freed when the device is removed.
 * No need to free the private data structure because devres takes care of that.
 * Return:
 *
 * 0 on success.
 */
static int ffa_remove(struct udevice *dev)
{
	log_info("[FFA] removing the device\n");

	ffa_unmap_rxtx_buffers(dev);
	dev_set_priv(dev, NULL);

	return 0;
}

/**
 * ffa_unbind() - The driver unbind function
 * @dev:	the arm_ffa device
 * Making sure the RX/TX buffers are unmapped and freed when the device is unbound.
 * No need to free the private data structure because devres takes care of that.
 * Return:
 *
 * 0 on success.
 */
static int ffa_unbind(struct udevice *dev)
{
	struct ffa_priv *priv = dev_get_priv(dev);

	log_info("[FFA] unbinding the device\n");

	if (priv)
		ffa_unmap_rxtx_buffers(dev);

	return 0;
}

/* FF-A driver operations */

static const struct ffa_bus_ops ffa_ops = {
	.partition_info_get = ffa_get_partitions_info,
	.sync_send_receive = ffa_msg_send_direct_req,
	.rxtx_unmap = ffa_unmap_rxtx_buffers,
};

/* Declaring the FF-A driver under UCLASS_FFA */

U_BOOT_DRIVER(arm_ffa) = {
	.name		= FFA_DRV_NAME,
	.id		= UCLASS_FFA,
	.flags		= DM_REMOVE_OS_PREPARE,
	.probe		= ffa_probe,
	.remove	= ffa_remove,
	.unbind	= ffa_unbind,
	.ops		= &ffa_ops,
};
