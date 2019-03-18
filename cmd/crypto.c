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
#include <u-boot/rsa-mod-exp.h>
/*
 * avoid duplicated inclusion:
 * #include "../lib/crypto/x509_parser.h"
 */
#include "../lib/crypto/pkcs7_parser.h"

static struct x509_certificate *cur_cert;
static struct pkcs7_message *cur_message;

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
		print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
			       cert->pub->key, cert->pub->keylen,
			       false);
		printf("    id_type: %s\n", cert->pub->id_type);
		printf("    pkey algo: %s\n", cert->pub->pkey_algo);
		prop = rsa_gen_key_prop(cert->pub->key, cert->pub->keylen);
		if (prop) {
			printf("    parameters:\n");
			printf("        exp_len: %d\n", prop->exp_len);
			print_hex_dump("        ", DUMP_PREFIX_OFFSET, 16, 1,
				       prop->public_exponent, prop->exp_len,
				       false);
			printf("        num_bits(mod len): %d\n",
			       prop->num_bits);
			printf("        modulus: %p (%x)\n", prop->modulus,
			       *(uint32_t *)prop->modulus);
			print_hex_dump("        ", DUMP_PREFIX_OFFSET, 16, 1,
				       prop->modulus,
				       (prop->num_bits + 7) >> 3,
				       false);
			printf("        n0inv: %x\n", prop->n0inv);
			printf("        rr: %p (%x)\n", prop->rr,
			       *(uint32_t *)prop->rr);
			print_hex_dump("        ", DUMP_PREFIX_OFFSET, 16, 1,
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

void print_pkcs7_signed_info(struct pkcs7_signed_info *info)
{
	struct rtc_time time;

	printf("    next: %p\n", info->next);
	printf("    signer: %p\n", info->signer);
	printf("    msgdigest_len: %x\n",
	       info->msgdigest_len);
	print_hex_dump("    ", DUMP_PREFIX_OFFSET, 16, 1,
		       info->msgdigest,
		       info->msgdigest_len, false);
	printf("    authattrs_len: %x\n",
	       info->authattrs_len);
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
	printf("    pkey sig: %p\n", info->sig);
	if (info->sig) {
		printf("        signature:%p\n", info->sig->s);
		printf("        sig size:%x\n", info->sig->s_size);
		printf("        digest:%p\n", info->sig->digest);
		printf("        digest size:%x\n", info->sig->digest_size);
		printf("        pkey algo:%s\n", info->sig->pkey_algo);
		printf("        hash algo:%s\n", info->sig->hash_algo);
		printf("        encoding:%s\n", info->sig->encoding);
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
	"crypto verify\n"
	"  - verify signature loaded at previous command\n"
	;
#endif

U_BOOT_CMD(
	crypto, CONFIG_SYS_MAXARGS, 0, do_crypto,
	"Handle signature verification",
	crypto_help_text
);
