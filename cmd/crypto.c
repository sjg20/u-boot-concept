// SPDX-License-Identifier: GPL-2.0+
/*
 *  Crypto command
 *
 *  Copyright (c) 2019 AKASHI Takahiro, Linaro Limited
 */

#include <common.h>
#include <command.h>
#include <efi_loader.h>
#include <exports.h>
#include <hexdump.h>
#include <malloc.h>
#include <mapmem.h>
#include <pe.h>
#include <rtc.h>
#include <linux/compat.h>
#include <u-boot/rsa-mod-exp.h>
/*
 * avoid duplicated inclusion:
 * #include "../lib/crypto/x509_parser.h"
 */
#include "../lib/crypto/pkcs7_parser.h"
#include "../lib/crypto/tstinfo_parser.h"
#if 1 /* DEBUG */
#include <hash.h>
#endif

static struct x509_certificate *cur_cert;
static struct pkcs7_message *cur_message;
static int dump_data;

struct oid_table {
	enum OID oid;
	const char *name;
} oid_table[] = {
	{OID_sha1, "sha1"},
	{OID_sha256, "sha256"},
};

static const char *oid_to_name(enum OID oid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(oid_table); i++)
		if (oid_table[i].oid == oid)
			return oid_table[i].name;

	return "Unknown";
}

void print_x509_certificate(struct x509_certificate *cert)
{
	struct key_prop *prop;
	struct rtc_time time_from, time_to;

	printf("signer: %p\n", cert->signer);
	if (cert->signer) {
		printf("   issuer: %p\n", cert->signer->issuer);
		printf("   subject: %p\n", cert->signer->subject);
	}
	printf("   verified: %s\n", cert->verified ? "true" : "false");
	printf("   self-signed: %s\n",
	       cert->self_signed ? "true" : "false");
	printf("issuer: %s\n", cert->issuer);
	printf("subject: %s\n", cert->subject);
	printf("public key: %p\n", cert->pub);
	if (cert->pub) {
		printf("    keylen: %x\n", cert->pub->keylen);
		if (dump_data)
			print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
				       cert->pub->key, cert->pub->keylen,
			       false);
		printf("    id_type: %s\n", cert->pub->id_type);
		printf("    pkey algo: %s\n", cert->pub->pkey_algo);
		prop = rsa_gen_key_prop(cert->pub->key, cert->pub->keylen);
		if (prop) {
			printf("    parameters:\n");
			printf("        exp_len: %d\n", prop->exp_len);
			if (dump_data)
				print_hex_dump("        ", DUMP_PREFIX_OFFSET,
					       16, 1,
					       prop->public_exponent,
					       prop->exp_len, false);
			printf("        num_bits(mod len): %d\n",
			       prop->num_bits);
			printf("        modulus: %p (%x)\n", prop->modulus,
			       *(uint32_t *)prop->modulus);
			if (dump_data)
				print_hex_dump("        ", DUMP_PREFIX_OFFSET,
					       16, 1,
					       prop->modulus,
					       (prop->num_bits + 7) >> 3,
					       false);
			printf("        n0inv: %x\n", prop->n0inv);
			printf("        rr: %p (%x)\n", prop->rr,
			       *(uint32_t *)prop->rr);
			if (dump_data)
				print_hex_dump("        ", DUMP_PREFIX_OFFSET,
					       16, 1,
					       prop->rr,
					       (prop->num_bits + 7) >> 3,
					       false);
		}
	}
	printf("signature params: %p\n", cert->sig);
	if (cert->sig) {
		printf("    signature size: %x\n", cert->sig->s_size);
		if (dump_data)
			print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
				       cert->sig->s, cert->sig->s_size, false);
		printf("    digest size: %x\n", cert->sig->digest_size);
		if (dump_data)
			print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
				       cert->sig->digest,
				       cert->sig->digest_size, false);
		printf("    pkey algo: %s\n", cert->sig->pkey_algo);
		printf("    hash algo: %s\n", cert->sig->hash_algo);
		printf("    encoding: %s\n", cert->sig->encoding);
	}
	rtc_to_tm(cert->valid_from, &time_from);
	rtc_to_tm(cert->valid_to, &time_to);
	printf("valid: from %04d-%02d-%02d %02d:%02d:%02d to %04d-%02d-%02d %02d:%02d:%02d\n",
	       time_from.tm_year, time_from.tm_mon, time_from.tm_mday,
	       time_from.tm_hour, time_from.tm_min, time_from.tm_sec,
	       time_to.tm_year, time_to.tm_mon, time_to.tm_mday,
	       time_to.tm_hour, time_to.tm_min, time_to.tm_sec);
	printf("tbs: %p, size: %x\n", cert->tbs, cert->tbs_size);
}

static void print_tstinfo(struct tstinfo *info)
{
	struct extension *ext;
	struct rtc_time time;
	int i;

	printf("    version: %x\n", info->version);
	printf("    policy: %x\n", info->policy);
	printf("    digest:\n");
	printf("      algo: %s(%x)\n", oid_to_name(info->digest.algo),
	       info->digest.algo);
	printf("      len: %lx\n", info->digest.size);
	if (dump_data)
		print_hex_dump("      ", DUMP_PREFIX_OFFSET, 16, 1,
			       info->digest.data, info->digest.size, false);
	if (info->serial_hi)
		printf("    serial#: %llx%016llx\n",
		       info->serial_hi, info->serial_lo);
	else
		printf("    serial#: %llx\n", info->serial_lo);
	rtc_to_tm(info->time, &time);
	printf("    time: %04d-%02d-%02d %02d:%02d:%02d\n",
	       time.tm_year, time.tm_mon, time.tm_mday,
	       time.tm_hour, time.tm_min, time.tm_sec);
	printf("    accuracy: %d:%d:%d\n",
	       info->accuracy.sec, info->accuracy.msec, info->accuracy.usec);
	printf("    tsa: %p, size: %lx\n", info->tsa.data, info->tsa.size);
	if (info->tsa.size && dump_data)
		print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
			       info->tsa.data, info->tsa.size, true);
	for (i = 0, ext = info->ext_next; ext; i++, ext = ext->next) {
		printf("    ext[%d]: %x\n", i, ext->oid);
		printf("      crit:%d\n", ext->critical);
		printf("      data:%p, size:%lx\n", ext->data, ext->size);
	}
}

void print_pkcs7_signed_info(struct pkcs7_signed_info *info)
{
	struct rtc_time time;

	printf("    next: %p\n", info->next);
	printf("    signer: %p\n", info->signer);
	printf("    authattrs_len: %x\n",
	       info->authattrs_len);
	if (info->authattrs_len && dump_data)
		print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
			       info->authattrs,
			       info->authattrs_len, false);
	printf("    aa_set: %lx\n", info->aa_set);
	printf("      Content Type: %s\n", test_bit(sinfo_has_content_type,
						    &info->aa_set) ?
						"true" : "false");
	printf("      Signing Time: %s\n", test_bit(sinfo_has_signing_time,
						    &info->aa_set) ?
						"true" : "false");
	printf("      Message Digest: %s\n", test_bit(sinfo_has_message_digest,
						      &info->aa_set) ?
						"true" : "false");
	printf("      Smime Caps: %s\n", test_bit(sinfo_has_smime_caps,
						  &info->aa_set) ?
						"true" : "false");
	printf("      MS Opus Info: %s\n", test_bit(sinfo_has_ms_opus_info,
						    &info->aa_set) ?
						"true" : "false");
	printf("      MS Statement Type: %s\n",
	       test_bit(sinfo_has_ms_statement_type,
			&info->aa_set) ? "true" : "false");
	rtc_to_tm(info->signing_time, &time);
	printf("    time: %04d-%02d-%02d %02d:%02d:%02d\n",
	       time.tm_year, time.tm_mon, time.tm_mday,
	       time.tm_hour, time.tm_min, time.tm_sec);
	printf("    msgdigest in auth: len: %x\n",
	       info->msgdigest_len);
	if (info->msgdigest_len && dump_data)
		print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
			       info->msgdigest,
			       info->msgdigest_len, false);
	printf("    signature: %p\n", info->sig);
	if (info->sig) {
		printf("        encoding:%s\n", info->sig->encoding);
		printf("        digest algo:%s\n", info->sig->hash_algo);
		printf("        signature:%p\n", info->sig->s);
		printf("        signature size:%x\n", info->sig->s_size);
		printf("        sig enc algo:%s\n", info->sig->pkey_algo);
		printf("        calc'ed digest:%p\n", info->sig->digest);
		printf("        digest size:%x\n", info->sig->digest_size);
		if (dump_data)
			print_hex_dump("      ", DUMP_PREFIX_OFFSET, 16, 1,
				       info->sig->s, info->sig->s_size, false);
	}

	printf("    unauthenticated attr:\n");
	printf("      len: %x\n", info->unauthattrs_len);
	if (info->ua_next) {
		struct attribute *uattr;

		for (uattr = info->ua_next; uattr; uattr = uattr->next)
			printf("      oid:%x val:%p size:%lx\n",
			       uattr->oid, uattr->data, uattr->size);
	}

	if (info->counter_signature) {
		struct pkcs7_message *cnt_sig;
		struct tstinfo *tst;
		void print_pkcs7_message(struct pkcs7_message *);

		printf("=== counter signature of timestamp ===\n");
		cnt_sig = pkcs7_parse_message(info->counter_signature->data,
					      info->counter_signature->size);
		if (!IS_ERR(cnt_sig)) {
			print_pkcs7_message(cnt_sig);
			tst = tstinfo_parse(cnt_sig->data, cnt_sig->data_len);
			if (!IS_ERR(tst)) {
				print_tstinfo(tst);
			} else {
				printf("Err: parsing tstinfo failed.\n");
				goto out;
			}
#if 1 /* DEBUG */
	/*
	 * TODO:
	 * must be verified with one of TSA certificates.
	 * In this case, pkcs7's content is data to be signed.
	 */
{
	struct efi_image_regions regs;
	struct x509_certificate *cert;
	int i;
	extern bool efi_signature_verify(struct efi_image_regions *regs,
					 struct pkcs7_signed_info *info,
					 struct x509_certificate *cert,
					 struct efi_signature_store *untrusted);

	memset(&regs, 0, sizeof(regs));
	if (cnt_sig->data) {
		efi_image_region_add(&regs, cnt_sig->data,
				     cnt_sig->data + cnt_sig->data_len,
				     1);
	} else {
		printf("No content in signed Data\n");
		goto err;
	}

        for (cert = cnt_sig->certs, i = 1; cert; cert = cert->next, i++) {
		printf("Trying certifcate %d\n", i);
		if (!efi_signature_verify(&regs, cnt_sig->signed_infos,
					  cert, NULL)) {
			printf("Verifying counter signature failed\n");
		} else {
			printf("Verifying counter signature succeeded!\n");
			break;
		}
	}
	printf("End of verification: %d\n", i);

{
	u8 *buf;
	int len;
	const char *algo;

	/* TODO: check TSTInfo's digest */
	if (tst->digest.algo == OID_sha1) {
		algo = "sha1";
	} else if (tst->digest.algo == OID_sha256) {
		algo = "sha256";
	} else {
		printf("Checking TSTInfo: unknown digest type:0x%x\n",
		       tst->digest.algo);
		goto err2;
	}

	buf = malloc(HASH_MAX_DIGEST_SIZE);
	len = HASH_MAX_DIGEST_SIZE;
	hash_block(algo, info->sig->s, info->sig->s_size, buf, &len);
	printf("------hash(%s) of TSTInfo:\n", algo);
	print_hex_dump("      ", DUMP_PREFIX_OFFSET, 16, 1,
		       buf, len, false);
	printf("------hash in TSTInfo:\n");
	print_hex_dump("      ", DUMP_PREFIX_OFFSET, 16, 1,
		       tst->digest.data, tst->digest.size, false);
	printf("------end of TSTInfo check\n");
err2:
	;
}

err:
	;
}
#endif
out:
			tstinfo_free(tst);
			pkcs7_free_message(cnt_sig);
		} else {
			printf("Err: parsing counter signature failed.\n");
		}
	} else {
		printf("=== No counterSignature (timestamp) ===\n");
	}
}

void print_pkcs7_message(struct pkcs7_message *message)
{
	struct x509_certificate *cert;
	struct pkcs7_signed_info *info;
	int i;

	printf("version: %x\n", message->version);
	printf("content data:\n");
	printf("    data_type: %x\n", message->data_type);
	printf("    data_len: %lx\n", message->data_len);
	printf("    data_hdrlen: %lx\n", message->data_hdrlen);
	printf("=== certificate list ===\n");
	for (cert = message->certs, i = 1; cert; cert = cert->next, i++) {
		printf("--- certifcate (%d) ---\n", i);
		print_x509_certificate(cert);
	}
	printf("=== revokation list ===\n");
	for (cert = message->crl, i = 1; cert; cert = cert->next, i++) {
		printf("--- revokated certificate (%d) ---\n", i);
		print_x509_certificate(cert);
	}
	printf("=== signed infos ===\n");
	for (info = message->signed_infos, i = 1; info;
	     info = info->next, i++) {
		printf("--- signed info (%d) ---\n", i);
		printf("--- signed info (addr: %p) ---\n", info);
		print_pkcs7_signed_info(info);
	}
}

/**
 * do_crypto_pkcs7() - parse and dump pkcs7 message
 *
 * @cmdtp:	Command table
 * @flag:	Command flag
 * @argc:	Number of arguments
 * @argv:	Argument array
 * Return:	CMD_RET_SUCCESS on success, CMD_RET_RET_FAILURE on failure
 *
 * (description)
 */
static int do_crypto_pkcs7(cmd_tbl_t *cmdtp, int flag,
			   int argc, char * const argv[])
{
	void *data;
	size_t datalen;
	struct efi_image_regions *regs = NULL;
	WIN_CERTIFICATE *auth;
	size_t auth_len;
	int ret = CMD_RET_FAILURE;

	if (argc < 3)
		return CMD_RET_USAGE;

	data = (void *)(uintptr_t)simple_strtoul(argv[1], 0, 16);
	datalen = simple_strtoul(argv[2], 0, 16);
#ifdef CONFIG_SANDBOX
	data = map_sysmem((phys_addr_t)data, datalen);
#endif

	if (!efi_image_parse(data, datalen, &regs, &auth, &auth_len)) {
		printf("Err: parsing a file failed.\n");
		goto out;
	}

	/* TODO */
	printf("NOTE: currently the first windows certificate is shown\n");
	if (auth) {
		struct win_certificate_uefi_guid *hdr;
		hdr = (typeof(hdr))auth;
		printf("=== Windows header ===\n");
		printf("    win length: 0x%x\n", auth->dwLength);
		printf("    win revision: 0x%x\n", auth->wRevision);
		printf("    win certifcate_type: 0x%04x\n",
		       auth->wCertificateType);
		printf("    win cert_type: %pUl\n", &hdr->cert_type);
	} else {
		printf("No authentication data\n");
		ret = CMD_RET_SUCCESS;
		goto out;
	}

	free(cur_message);
	/* without a header */
	cur_message = pkcs7_parse_message((void *)&auth[1],
					  auth->dwLength - sizeof(*auth));
	if (!cur_message || IS_ERR(cur_message)) {
		printf("Err: parsing pkcs7 message failed.\n");
		goto out;
	}

	print_pkcs7_message(cur_message);
	ret = CMD_RET_SUCCESS;

out:
#ifdef CONFIG_SANDBOX
	unmap_sysmem(data);
#endif
	free(regs);

	return ret;
}

/**
 * do_crypto_x509() - parse and dump x509 certificate
 *
 * @cmdtp:	Command table
 * @flag:	Command flag
 * @argc:	Number of arguments
 * @argv:	Argument array
 * Return:	CMD_RET_SUCCESS on success, CMD_RET_RET_FAILURE on failure
 *
 * (description)
 */
static int do_crypto_x509(cmd_tbl_t *cmdtp, int flag,
			  int argc, char * const argv[])
{
	void *data;
	size_t datalen;
	struct x509_certificate *cert;
	int i;

	if (argc < 3)
		return CMD_RET_USAGE;

	data = (void *)simple_strtoul(argv[1], 0, 16);
	datalen = simple_strtoul(argv[2], 0, 16);
#ifdef CONFIG_SANDBOX
	data = map_sysmem((phys_addr_t)data, datalen);
#endif

	free(cur_cert);
	cur_cert = x509_cert_parse(data, datalen);
#ifdef CONFIG_SANDBOX
	unmap_sysmem(data);
#endif
	if (!cur_cert || IS_ERR(cur_cert)) {
		printf("Err: parsing x509 failed.\n");
		return CMD_RET_FAILURE;
	}

	printf("next: %p\n", cur_cert->next);
	for (cert = cur_cert, i = 1; cert; cert = cert->next, i++) {
		printf("--- certificate (%d) ---\n", i);
		print_x509_certificate(cert);
	}

	return CMD_RET_SUCCESS;
}

#if 0 /* FIXME */
/**
 * dump_efi_signature() - show information about signatures
 *
 * @prefix:	Prefix string
 * @sigstore:	List of signatures
 *
 * Iterate signatures and show information of each signature
 */
static void dump_efi_signature(const char *prefix,
			       struct efi_signature_store *sigstore)
{
	efi_signature *sig;
	struct x509_certificate *certs, *cert;
	extern void print_x509_certificate(struct x509_certificate *);
	int i;

	for (i = 0; i < sigstore->num; i++) {
		printf("%s--- signature (%d) ---\n", prefix, i + 1);
		sig = &sigstore->sig[i];
		printf("%stype: %s\n", prefix, efi_guid_to_str(&sig->type));
		printf("%sdata size: 0x%lx\n", prefix, sig->datalen);
		printf("%sattrs size: 0x%lx\n", prefix, sig->attrslen);
		printf("%sdigest size: 0x%lx\n", prefix, sig->digestlen);

		if (!guidcmp(&sig->type, &efi_guid_cert_x509)) {
			printf(">>> as x509 certificate\n");
			certs = x509_cert_parse(sig->data, sig->datalen);
			if (certs) {
				for (cert = certs; cert; cert = cert->next)
					print_x509_certificate(cert);
				x509_free_certificate(certs);
			}
			printf("<<<\n");
		}
	}
}

/**
 * parse_and_print_authinfo() - parse and show information about
 * variable's authentication data
 *
 * @data:	Address of data
 * @data_size:	Size of data
 * @verbose:	if true, dump signature database
 * Return:	0 on success, error code on failure
 *
 * Parse the data in pkcs7 SignedData and return 0 if no format problem,
 * optionally show information as variable's authentication data
 * if verbose is specified
 */
static int parse_and_print_authinfo(u8 **data, efi_uintn_t *data_size,
				    bool verbose)
{
	struct efi_variable_authentication_2 *auth;
	struct efi_signature_store *sigstore;
	struct efi_time *timestamp;
	efi_status_t ret;

	sigstore = calloc(sizeof(*sigstore), 1);
	if (!sigstore) {
		printf("## Out of memory\n");
		return -ENOMEM;
	}

	if (*data_size < sizeof(struct efi_variable_authentication_2))
		goto err;

	auth = (typeof(auth))*data;
	if (*data_size < (sizeof(auth->time_stamp)
				+ auth->auth_info.hdr.dwLength))
		goto err;

	if (verbose)
		printf("    signature type: %s\n",
		       efi_guid_to_str(&auth->auth_info.cert_type));
	if (guidcmp(&auth->auth_info.cert_type, &efi_guid_cert_type_pkcs7))
		goto err;

	timestamp = &auth->time_stamp;
	if (verbose)
		printf("    signed time: %02d-%02d-%02d %02d:%02d:%02d\n",
		       timestamp->year, timestamp->month, timestamp->day,
		       timestamp->hour, timestamp->minute, timestamp->second);

	if (auth->auth_info.hdr.dwLength < sizeof(auth->auth_info))
		goto err;
	ret = efi_sigstore_parse_variable_authdata(sigstore,
						   auth->auth_info.cert_data,
						   auth->auth_info.hdr.dwLength
						    - sizeof(auth->auth_info));
	if (ret != EFI_SUCCESS)
		goto err;

	if (verbose)
		dump_efi_signature("    ", sigstore);
	efi_sigstore_free(sigstore);

	*data += sizeof(auth->time_stamp) + auth->auth_info.hdr.dwLength;
	*data_size -= (sizeof(auth->time_stamp)
				+ auth->auth_info.hdr.dwLength);

	return 0;

err:
	printf("Parsing time-based signature data failed\n");

	return -EINVAL;
}
#endif

/**
 * do_crypto_db() - dump signature database
 *
 * @cmdtp:	Command table
 * @flag:	Command flag
 * @argc:	Number of arguments
 * @argv:	Argument array
 * Return:	CMD_RET_SUCCESS on success, CMD_RET_RET_FAILURE on failure
 *
 * (description)
 */
static int do_crypto_db(cmd_tbl_t *cmdtp, int flag,
			int argc, char * const argv[])
{
	u16 *name, *p;
	struct efi_signature_store *sigstore, *siglist;
	struct efi_sig_data *sig_data;
	struct x509_certificate *certs, *cert;
	int i, j, k;
	efi_status_t ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	ret = efi_init_obj_list();
	if (ret != EFI_SUCCESS) {
		printf("Cannot initialize UEFI sub-system, ret = %lu\n",
		       ret & ~EFI_ERROR_MASK);
		return CMD_RET_FAILURE;
	}

	name = malloc(utf8_utf16_strnlen(argv[1], strlen(argv[1])) + 2);
	if (!name) {
		printf("out of memory\n");
		return CMD_RET_FAILURE;
	}
	p = name;
	utf8_utf16_strncpy(&p, argv[1], strlen(argv[1]) + 1);

	sigstore = efi_sigstore_parse_sigdb(name);
	free(name);
	if (!sigstore) {
		printf("retrieving \"%s\" failed\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	for (siglist = sigstore, i = 1; siglist; siglist = siglist->next, i++) {
		printf("=== signature list (%d) ===\n", i);
		if (!guidcmp(&siglist->sig_type, &efi_guid_cert_x509))
			printf("type: x509 certificate\n");
		else
			/* TODO */
			printf("type: signature\n");

		for (sig_data = siglist->sig_data_list, j = 1; sig_data;
		     sig_data = sig_data->next, j++) {
			printf("  === signature (%d) ===\n", j);
			printf("  owner: %pUl\n", &sig_data->owner);

			if (!guidcmp(&siglist->sig_type, &efi_guid_cert_x509)) {
				certs = x509_cert_parse(sig_data->data,
							sig_data->size);
				if (!certs) {
					printf("parsing certificate failed\n");
					continue;
				}

				for (cert = certs, k = 1; cert;
				     cert = cert->next, k++) {
					printf("  --- certificate (%d) ---\n",
					       k);
					print_x509_certificate(cert);
				}
				x509_free_certificate(certs);
			} else {
				/* TODO */
				print_hex_dump("  ", DUMP_PREFIX_OFFSET,
					       16, 1,
					       sig_data->data, sig_data->size,
					       false);
			}
		}
	}
	efi_sigstore_free(sigstore);

	return CMD_RET_SUCCESS;
}

/**
 * do_crypto_verify() - verify signature
 *
 * @cmdtp:	Command table
 * @flag:	Command flag
 * @argc:	Number of arguments
 * @argv:	Argument array
 * Return:	CMD_RET_SUCCESS on success, CMD_RET_RET_FAILURE on failure
 *
 * (description)
 */
static int do_crypto_verify(cmd_tbl_t *cmdtp, int flag,
			    int argc, char * const argv[])
{
	if (!cur_cert)
		printf("Err: Certificates not loaded.\n");

	if (!cur_message)
		printf("Err: Message to be verified not loaded.\n");

	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_crypto_sub[] = {
	U_BOOT_CMD_MKENT(pkcs7, CONFIG_SYS_MAXARGS, 1, do_crypto_pkcs7, "", ""),
	U_BOOT_CMD_MKENT(x509, CONFIG_SYS_MAXARGS, 1, do_crypto_x509,
			 "", ""),
	U_BOOT_CMD_MKENT(db, CONFIG_SYS_MAXARGS, 1, do_crypto_db, "", ""),
	U_BOOT_CMD_MKENT(verify, CONFIG_SYS_MAXARGS, 1, do_crypto_verify,
			 "", ""),
};

/**
 * do_crypto() - handle signature verification
 *
 * @cmdtp:	Command table
 * @flag:	Command flag
 * @argc:	Number of arguments
 * @argv:	Argument array
 * Return:	CMD_RET_SUCCESS on success,
 *		CMD_RET_USAGE or CMD_RET_RET_FAILURE on failure
 *
 * (description)
 */
static int do_crypto(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	cmd_tbl_t *cp;

	if (argc < 2)
		return CMD_RET_USAGE;

	argc--; argv++;

	cp = find_cmd_tbl(argv[0], cmd_crypto_sub, ARRAY_SIZE(cmd_crypto_sub));
	if (!cp)
		return CMD_RET_USAGE;

	return cp->cmd(cmdtp, flag, argc, argv);
}

#ifdef CONFIG_SYS_LONGHELP
static char crypto_help_text[] =
	"  - Handle signature verification\n"
	"\n"
	"crypto pkcs7 <address> <size>\n"
	"  - parse and dump pkcs7 message\n"
	"crypto x509 <address> <size>\n"
	"  - parse and dump x509 certificate\n"
	"crypto db <database>\n"
	"  - dump signature database\n"
	"crypto verify\n"
	"  - verify signature loaded at previous command\n"
	;
#endif

U_BOOT_CMD(
	crypto, CONFIG_SYS_MAXARGS, 0, do_crypto,
	"Handle signature verification",
	crypto_help_text
);
