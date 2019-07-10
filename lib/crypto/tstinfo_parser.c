// SPDX-License-Identifier: GPL-2.0+
/*
 * TSTInfo Parser
 *
 * Copyright (c) 2019 Linaro Limited
 *			Author: AKASHI Takahiro
 */

#include <malloc.h>
#include <linux/err.h>
#include <linux/oid_registry.h>
#include "tstinfo_parser.h"
#include "tstinfo.asn1.h"

struct tstinfo_parse_context {
	struct tstinfo *info;
	/* internal use */
	unsigned long data;
	enum OID last_oid;
	bool critical;
};

void tstinfo_free(struct tstinfo *info)
{
	struct extension *ext, *ext_next;

	for (ext = info->ext_next; ext;) {
		if (ext)
			ext_next = ext->next;
		free(ext);
		ext = ext_next;
	}
	free(info);
}

struct tstinfo *tstinfo_parse(const void *data, size_t datalen)
{
	struct tstinfo_parse_context *ctx = NULL;
	struct tstinfo *info = ERR_PTR(-ENOMEM);
	int ret;

	ctx = calloc(sizeof(*ctx), 1);
	if (!ctx)
		goto err;
	info = calloc(sizeof(*info), 1);
	if (!info)
		goto err;

	ctx->info = info;
	ctx->data = (unsigned long)data;

	ret = asn1_ber_decoder(&tstinfo_decoder, ctx, data, datalen);
	if (ret < 0) {
		tstinfo_free(info);
		info = ERR_PTR(ret);
	} else {
		info = ctx->info;
	}

err:
	free(ctx);

	return info;
}

int tstinfo_note_version(void *context, size_t hdrlen, unsigned char tag,
			 const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	ctx->info->version = *(const u8 *)value;

	return 0;
}

int tstinfo_note_policy(void *context, size_t hdrlen, unsigned char tag,
			const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	ctx->last_oid = look_up_OID(value, vlen);
	if (ctx->last_oid == OID__NR) {
		char buffer[50];
		sprint_oid(value, vlen, buffer, sizeof(buffer));
		printf("TSTInfo: Unknown PolicyID: [%lu] %s\n",
		       (unsigned long)value - ctx->data, buffer);
	}

	return 0;
}

int tstinfo_note_OID(void *context, size_t hdrlen, unsigned char tag,
		     const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	ctx->last_oid = look_up_OID(value, vlen);
	if (ctx->last_oid == OID__NR) {
		char buffer[50];
		sprint_oid(value, vlen, buffer, sizeof(buffer));
		printf("TSTInfo: Unknown OID: [%lu] %s\n",
		       (unsigned long)value - ctx->data, buffer);
	}

	return 0;
}

int tstinfo_note_hash_algo(void *context, size_t hdrlen, unsigned char tag,
			   const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	ctx->info->digest.algo = ctx->last_oid;

	return 0;
}

int tstinfo_note_hash_msg(void *context, size_t hdrlen, unsigned char tag,
			  const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	/* TODO: remove a header? */
	ctx->info->digest.data = value;
	ctx->info->digest.size = vlen;

	return 0;
}

static u64 tstinfo_to_u64(const void *value, size_t len)
{
	u64 v = 0;
	const u8 *p = value;

	while (len) {
		v = (v << 8) + *p;
		len--;
		p++;
	}

	return v;
}

int tstinfo_note_serial(void *context, size_t hdrlen, unsigned char tag,
			const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	if (vlen > 8) {
		ctx->info->serial_hi = tstinfo_to_u64(value, vlen - 8);
		value += vlen - 8;
		vlen = 8;
	}
	ctx->info->serial_lo = tstinfo_to_u64(value, vlen);

	return 0;
}

/* from x509_decode_time() */
#define dec2bin(X) ({ unsigned char x = (X) - '0'; \
			if (x > 9) return -1; x; })
#define DD2bin(P) ({ unsigned x = dec2bin(P[0]) * 10 + dec2bin(P[1]); \
			P += 2; x; })

int tstinfo_note_time(void *context, size_t hdrlen, unsigned char tag,
		      const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;
	static const unsigned char month_lengths[] = { 31, 28, 31, 30, 31, 30,
						       31, 31, 30, 31, 30, 31 };
	const unsigned char *p = value;
	unsigned year, mon, day, hour, min, sec, mon_len;

	/* GeneralizedTime: YYYYMMDDHHMMSSZ or YYYYMMDDHHMMSS.FFFZ   */
	if (vlen != 15 && vlen != 19)
		return -1;

	year = DD2bin(p) * 100 + DD2bin(p);
	mon  = DD2bin(p);
	day = DD2bin(p);
	hour = DD2bin(p);
	min  = DD2bin(p);
	sec  = DD2bin(p);

	if (*p != 'Z' && *p != '.')
		return -1;

	if (year < 1970 ||
	    mon < 1 || mon > 12)
		return -1;

	mon_len = month_lengths[mon - 1];
	if (mon == 2) {
		if (year % 4 == 0) {
			mon_len = 29;
			if (year % 100 == 0) {
				mon_len = 28;
				if (year % 400 == 0)
					mon_len = 29;
			}
		}
	}

	if (day < 1 || day > mon_len ||
	    hour > 24 || /* ISO 8601 permits 24:00:00 as midnight tomorrow */
	    min > 59 ||
	    sec > 60) /* ISO 8601 permits leap seconds [X.680 46.3] */
		return -1;

	ctx->info->time = mktime64(year, mon, day, hour, min, sec);

	return 0;
}

int tstinfo_note_accuracy(void *context, size_t hdrlen, unsigned char tag,
			  const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;
	int v;

	v = tstinfo_to_u64(value, vlen);
	if (v < 0)
		return -1;

	tag &= 0x3;
	if ((tag == 0 || tag == 1) && v > 999)
		return -1;

	/* FIXME: index? */
	/* TODO: milli & micro <= 999 */
	if (tag == 1)
		ctx->info->accuracy.msec = v;
	else if (tag == 2)
		ctx->info->accuracy.usec = v;
	else
		ctx->info->accuracy.sec = v;

	return 0;
}

int tstinfo_note_tsa(void *context, size_t hdrlen, unsigned char tag,
		     const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	ctx->info->tsa.data = value;
	ctx->info->tsa.size = vlen;

	return 0;
}

int tstinfo_note_ext_crit(void *context, size_t hdrlen, unsigned char tag,
			  const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;

	/* TODO: size of bool? */
	ctx->critical = *(u8 *)value;

	return 0;
}

int tstinfo_note_extension(void *context, size_t hdrlen, unsigned char tag,
			   const void *value, size_t vlen)
{
	struct tstinfo_parse_context *ctx = context;
	struct extension *ext;

	ext = calloc(sizeof(*ext), 1);
	if (!ext)
		return -1;

	ext->oid = ctx->last_oid;
	ext->critical = ctx->critical;
	ext->data = value;
	ext->size = vlen;
	ext->next = ctx->info->ext_next;
	ctx->info->ext_next = ext;

	return 0;
}
