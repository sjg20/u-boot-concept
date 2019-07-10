/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * TSTInfo Parser
 *
 * Copyright (c) 2019 Linaro Limited
 *			Author: AKASHI Takahiro
 */

#ifndef _CRYPTO_TSTINFO_H
#define _CRYPTO_TSTINFO_H

struct tstinfo;

extern struct tstinfo *tstinfo_parse(const void *data, size_t datalen);
extern void tstinfo_free(struct tstinfo *tstinfo);
#endif /* _CRYPTO_TSTINFO_H */
