// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2018 Google, Inc
 */

#include <common.h>
#include <vb2_api.h>

/* No-op stubs that can be overridden by SoCs with hardware crypto support */
int vb2ex_hwcrypto_digest_init(enum vb2_hash_algorithm hash_alg,
			       uint32_t data_size)
{
	return VB2_ERROR_EX_HWCRYPTO_UNSUPPORTED;
}

int vb2ex_hwcrypto_digest_extend(const uint8_t *buf, uint32_t size)
{
	return VB2_ERROR_EX_HWCRYPTO_UNSUPPORTED;
}

int vb2ex_hwcrypto_digest_finalize(uint8_t *digest, uint32_t digest_size)
{
	return VB2_ERROR_EX_HWCRYPTO_UNSUPPORTED;
}
