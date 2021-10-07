// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2018 Linaro Limited
 *		Author: AKASHI Takahiro
 */

#include <getopt.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include <linux/kconfig.h>
#ifdef CONFIG_TOOLS_LIBCRYPTO
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#endif

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;
typedef __s16 s16;
typedef __s32 s32;

#define aligned_u64 __aligned_u64

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#include <efi.h>
#include <efi_api.h>

static const char *tool_name = "mkeficapsule";

efi_guid_t efi_guid_fm_capsule = EFI_FIRMWARE_MANAGEMENT_CAPSULE_ID_GUID;
efi_guid_t efi_guid_image_type_uboot_fit =
		EFI_FIRMWARE_IMAGE_TYPE_UBOOT_FIT_GUID;
efi_guid_t efi_guid_image_type_uboot_raw =
		EFI_FIRMWARE_IMAGE_TYPE_UBOOT_RAW_GUID;
efi_guid_t efi_guid_cert_type_pkcs7 = EFI_CERT_TYPE_PKCS7_GUID;

#ifdef CONFIG_TOOLS_LIBCRYPTO
static const char *opts_short = "frg:i:I:v:p:c:m:dh";
#else
static const char *opts_short = "frg:i:I:v:h";
#endif

static struct option options[] = {
	{"fit", no_argument, NULL, 'f'},
	{"raw", no_argument, NULL, 'r'},
	{"guid", required_argument, NULL, 'g'},
	{"index", required_argument, NULL, 'i'},
	{"instance", required_argument, NULL, 'I'},
#ifdef CONFIG_TOOLS_LIBCRYPTO
	{"private-key", required_argument, NULL, 'p'},
	{"certificate", required_argument, NULL, 'c'},
	{"monotonic-count", required_argument, NULL, 'm'},
	{"dump-sig", no_argument, NULL, 'd'},
#endif
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static void print_usage(void)
{
	printf("Usage: %s [options] <image blob> <output file>\n"
	       "Options:\n"

	       "\t-f, --fit                   FIT image type\n"
	       "\t-r, --raw                   raw image type\n"
	       "\t-g, --guid <guid string>    guid for image blob type\n"
	       "\t-i, --index <index>         update image index\n"
	       "\t-I, --instance <instance>   update hardware instance\n"
#ifdef CONFIG_TOOLS_LIBCRYPTO
	       "\t-p, --private-key <privkey file>  private key file\n"
	       "\t-c, --certificate <cert file>     signer's certificate file\n"
	       "\t-m, --monotonic-count <count>     monotonic count\n"
	       "\t-d, --dump_sig              dump signature (*.p7)\n"
#endif
	       "\t-h, --help                  print a help message\n",
	       tool_name);
}

/**
 * auth_context - authentication context
 * @key_file:	Path to a private key file
 * @cert_file:	Path to a certificate file
 * @image_data:	Pointer to firmware data
 * @image_size:	Size of firmware data
 * @auth:	Authentication header
 * @sig_data:	Signature data
 * @sig_size:	Size of signature data
 *
 * Data structure used in create_auth_data(). @key_file through
 * @image_size are input parameters. @auth, @sig_data and @sig_size
 * are filled in by create_auth_data().
 */
struct auth_context {
	char *key_file;
	char *cert_file;
	u8 *image_data;
	size_t image_size;
	struct efi_firmware_image_authentication auth;
	u8 *sig_data;
	size_t sig_size;
};

static int dump_sig;

#ifdef CONFIG_TOOLS_LIBCRYPTO
/**
 * fileio-read_pkey - read out a private key
 * @filename:	Path to a private key file
 *
 * Read out a private key file and parse it into "EVP_PKEY" structure.
 *
 * Return:
 * * Pointer to private key structure  - on success
 * * NULL - on failure
 */
static EVP_PKEY *fileio_read_pkey(const char *filename)
{
	EVP_PKEY *key = NULL;
	BIO *bio;

	bio = BIO_new_file(filename, "r");
	if (!bio)
		goto out;

	key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);

out:
	BIO_free_all(bio);
	if (!key) {
		printf("Can't load key from file '%s'\n", filename);
		ERR_print_errors_fp(stderr);
	}

	return key;
}

/**
 * fileio-read_cert - read out a certificate
 * @filename:	Path to a certificate file
 *
 * Read out a certificate file and parse it into "X509" structure.
 *
 * Return:
 * * Pointer to certificate structure  - on success
 * * NULL - on failure
 */
static X509 *fileio_read_cert(const char *filename)
{
	X509 *cert = NULL;
	BIO *bio;

	bio = BIO_new_file(filename, "r");
	if (!bio)
		goto out;

	cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);

out:
	BIO_free_all(bio);
	if (!cert) {
		printf("Can't load certificate from file '%s'\n", filename);
		ERR_print_errors_fp(stderr);
	}

	return cert;
}

/**
 * create_auth_data - compose authentication data in capsule
 * @auth_context:	Pointer to authentication context
 *
 * Fill up an authentication header (.auth) and signature data (.sig_data)
 * in @auth_context, using library functions from openssl.
 * All the parameters in @auth_context must be filled in by a caller.
 *
 * Return:
 * * 0  - on success
 * * -1 - on failure
 */
static int create_auth_data(struct auth_context *ctx)
{
	EVP_PKEY *key = NULL;
	X509 *cert = NULL;
	BIO *data_bio = NULL;
	const EVP_MD *md;
	PKCS7 *p7;
	int flags, ret = -1;

	OpenSSL_add_all_digests();
	OpenSSL_add_all_ciphers();
	ERR_load_crypto_strings();

	key = fileio_read_pkey(ctx->key_file);
	if (!key)
		goto err;
	cert = fileio_read_cert(ctx->cert_file);
	if (!cert)
		goto err;

	/*
	 * create a BIO, containing:
	 *  * firmware image
	 *  * monotonic count
	 * in this order!
	 * See EDK2's FmpAuthenticatedHandlerRsa2048Sha256()
	 */
	data_bio = BIO_new(BIO_s_mem());
	BIO_write(data_bio, ctx->image_data, ctx->image_size);
	BIO_write(data_bio, &ctx->auth.monotonic_count,
		  sizeof(ctx->auth.monotonic_count));

	md = EVP_get_digestbyname("SHA256");
	if (!md)
		goto err;

	/* create signature */
	/* TODO: maybe add PKCS7_NOATTR and PKCS7_NOSMIMECAP */
	flags = PKCS7_BINARY | PKCS7_DETACHED;
	p7 = PKCS7_sign(NULL, NULL, NULL, data_bio, flags | PKCS7_PARTIAL);
	if (!p7)
		goto err;
	if (!PKCS7_sign_add_signer(p7, cert, key, md, flags))
		goto err;
	if (!PKCS7_final(p7, data_bio, flags))
		goto err;

	/* convert pkcs7 into DER */
	ctx->sig_data = NULL;
	ctx->sig_size = ASN1_item_i2d((ASN1_VALUE *)p7, &ctx->sig_data,
				      ASN1_ITEM_rptr(PKCS7));
	if (!ctx->sig_size)
		goto err;

	/* fill auth_info */
	ctx->auth.auth_info.hdr.dwLength = sizeof(ctx->auth.auth_info)
						+ ctx->sig_size;
	ctx->auth.auth_info.hdr.wRevision = WIN_CERT_REVISION_2_0;
	ctx->auth.auth_info.hdr.wCertificateType = WIN_CERT_TYPE_EFI_GUID;
	memcpy(&ctx->auth.auth_info.cert_type, &efi_guid_cert_type_pkcs7,
	       sizeof(efi_guid_cert_type_pkcs7));

	ret = 0;
err:
	BIO_free_all(data_bio);
	EVP_PKEY_free(key);
	X509_free(cert);

	return ret;
}

/**
 * dump_signature - dump out a signature
 * @path:	Path to a capsule file
 * @signature:	Signature data
 * @sig_size:	Size of signature data
 *
 * Signature data pointed to by @signature will be saved into
 * a file whose file name is @path with ".p7" suffix.
 *
 * Return:
 * * 0  - on success
 * * -1 - on failure
 */
static int dump_signature(const char *path, u8 *signature, size_t sig_size)
{
	char *sig_path;
	FILE *f;
	size_t size;
	int ret = -1;

	sig_path = malloc(strlen(path) + 3 + 1);
	if (!sig_path)
		return ret;

	sprintf(sig_path, "%s.p7", path);
	f = fopen(sig_path, "w");
	if (!f)
		goto err;

	size = fwrite(signature, 1, sig_size, f);
	if (size == sig_size)
		ret = 0;

	fclose(f);
err:
	free(sig_path);
	return ret;
}

/**
 * free_sig_data - free out signature data
 * @ctx:	Pointer to authentication context
 *
 * Free signature data allocated in create_auth_data().
 */
static void free_sig_data(struct auth_context *ctx)
{
	if (ctx->sig_size)
		OPENSSL_free(ctx->sig_data);
}
#else
static int create_auth_data(struct auth_context *ctx)
{
	return 0;
}

static int dump_signature(const char *path, u8 *signature, size_t sig_size)
{
	return 0;
}

static void free_sig_data(struct auth_context *ctx) {}
#endif

/**
 * create_fwbin - create an uefi capsule file
 * @path:	Path to a created capsule file
 * @bin:	Path to a firmware binary to encapsulate
 * @guid:	GUID of related FMP driver
 * @index:	Index number in capsule
 * @instance:	Instance number in capsule
 * @mcount:	Monotonic count in authentication information
 * @private_file:	Path to a private key file
 * @cert_file:	Path to a certificate file
 *
 * This function actually does the job of creating an uefi capsule file.
 * All the arguments must be supplied.
 * If either @private_file ror @cert_file is NULL, the capsule file
 * won't be signed.
 *
 * Return:
 * * 0  - on success
 * * -1 - on failure
 */
static int create_fwbin(char *path, char *bin, efi_guid_t *guid,
			unsigned long index, unsigned long instance,
			uint64_t mcount, char *privkey_file, char *cert_file)
{
	struct efi_capsule_header header;
	struct efi_firmware_management_capsule_header capsule;
	struct efi_firmware_management_capsule_image_header image;
	struct auth_context auth_context;
	FILE *f, *g;
	struct stat bin_stat;
	u8 *data;
	size_t size;
	u64 offset;

#ifdef DEBUG
	printf("For output: %s\n", path);
	printf("\tbin: %s\n\ttype: %pUl\n", bin, guid);
	printf("\tindex: %lu\n\tinstance: %lu\n", index, instance);
#endif
	auth_context.sig_size = 0;

	g = fopen(bin, "r");
	if (!g) {
		printf("cannot open %s\n", bin);
		return -1;
	}
	if (stat(bin, &bin_stat) < 0) {
		printf("cannot determine the size of %s\n", bin);
		goto err_1;
	}
	data = malloc(bin_stat.st_size);
	if (!data) {
		printf("cannot allocate memory: %zx\n", (size_t)bin_stat.st_size);
		goto err_1;
	}

	size = fread(data, 1, bin_stat.st_size, g);
	if (size < bin_stat.st_size) {
		printf("read failed (%zx)\n", size);
		goto err_2;
	}

	/* first, calculate signature to determine its size */
	if (privkey_file && cert_file) {
		auth_context.key_file = privkey_file;
		auth_context.cert_file = cert_file;
		auth_context.auth.monotonic_count = mcount;
		auth_context.image_data = data;
		auth_context.image_size = bin_stat.st_size;

		if (create_auth_data(&auth_context)) {
			printf("Signing firmware image failed\n");
			goto err_3;
		}

		if (dump_sig &&
		    dump_signature(path, auth_context.sig_data,
				   auth_context.sig_size)) {
			printf("Creating signature file failed\n");
			goto err_3;
		}
	}

	header.capsule_guid = efi_guid_fm_capsule;
	header.header_size = sizeof(header);
	/* TODO: The current implementation ignores flags */
	header.flags = CAPSULE_FLAGS_PERSIST_ACROSS_RESET;
	header.capsule_image_size = sizeof(header)
					+ sizeof(capsule) + sizeof(u64)
					+ sizeof(image)
					+ bin_stat.st_size;
	if (auth_context.sig_size)
		header.capsule_image_size += sizeof(auth_context.auth)
				+ auth_context.sig_size;

	f = fopen(path, "w");
	if (!f) {
		printf("cannot open %s\n", path);
		goto err_3;
	}

	size = fwrite(&header, 1, sizeof(header), f);
	if (size < sizeof(header)) {
		printf("write failed (%zx)\n", size);
		goto err_4;
	}

	capsule.version = 0x00000001;
	capsule.embedded_driver_count = 0;
	capsule.payload_item_count = 1;
	size = fwrite(&capsule, 1, sizeof(capsule), f);
	if (size < (sizeof(capsule))) {
		printf("write failed (%zx)\n", size);
		goto err_4;
	}
	offset = sizeof(capsule) + sizeof(u64);
	size = fwrite(&offset, 1, sizeof(offset), f);
	if (size < sizeof(offset)) {
		printf("write failed (%zx)\n", size);
		goto err_4;
	}

	image.version = 0x00000003;
	memcpy(&image.update_image_type_id, guid, sizeof(*guid));
	image.update_image_index = index;
	image.reserved[0] = 0;
	image.reserved[1] = 0;
	image.reserved[2] = 0;
	image.update_image_size = bin_stat.st_size;
	if (auth_context.sig_size)
		image.update_image_size += sizeof(auth_context.auth)
				+ auth_context.sig_size;
	image.update_vendor_code_size = 0; /* none */
	image.update_hardware_instance = instance;
	image.image_capsule_support = 0;
	if (auth_context.sig_size)
		image.image_capsule_support |= CAPSULE_SUPPORT_AUTHENTICATION;

	size = fwrite(&image, 1, sizeof(image), f);
	if (size < sizeof(image)) {
		printf("write failed (%zx)\n", size);
		goto err_4;
	}

	if (auth_context.sig_size) {
		size = fwrite(&auth_context.auth, 1,
			      sizeof(auth_context.auth), f);
		if (size < sizeof(auth_context.auth)) {
			printf("write failed (%zx)\n", size);
			goto err_4;
		}
		size = fwrite(auth_context.sig_data, 1,
			      auth_context.sig_size, f);
		if (size < auth_context.sig_size) {
			printf("write failed (%zx)\n", size);
			goto err_4;
		}
	}

	size = fwrite(data, 1, bin_stat.st_size, f);
	if (size < bin_stat.st_size) {
		printf("write failed (%zx)\n", size);
		goto err_4;
	}

	fclose(f);
	fclose(g);
	free(data);
	free_sig_data(&auth_context);

	return 0;

err_4:
	fclose(f);
err_3:
	free_sig_data(&auth_context);
err_2:
	free(data);
err_1:
	fclose(g);

	return -1;
}

/**
 * convert_uuid_to_guid() - convert uuid string to guid string
 * @buf:	String for UUID
 *
 * UUID and GUID have the same data structure, but their string
 * formats are different due to the endianness. See lib/uuid.c.
 * Since uuid_parse() can handle only UUID, this function must
 * be called to get correct data for GUID when parsing a string.
 *
 * The correct data will be returned in @buf.
 */
void convert_uuid_to_guid(unsigned char *buf)
{
	unsigned char c;

	c = buf[0];
	buf[0] = buf[3];
	buf[3] = c;
	c = buf[1];
	buf[1] = buf[2];
	buf[2] = c;

	c = buf[4];
	buf[4] = buf[5];
	buf[5] = c;

	c = buf[6];
	buf[6] = buf[7];
	buf[7] = c;
}

/**
 * main - main entry function of mkeficapsule
 * @argc:	Number of arguments
 * @argv:	Array of pointers to arguments
 *
 * Create an uefi capsule file, optionally signing it.
 * Parse all the arguments and pass them on to create_fwbin().
 *
 * Return:
 * * 0  - on success
 * * -1 - on failure
 */
int main(int argc, char **argv)
{
	efi_guid_t *guid;
	unsigned char uuid_buf[16];
	unsigned long index, instance;
	uint64_t mcount;
	char *privkey_file, *cert_file;
	int c, idx;

	guid = NULL;
	index = 0;
	instance = 0;
	mcount = 0;
	privkey_file = NULL;
	cert_file = NULL;
	dump_sig = 0;
	for (;;) {
		c = getopt_long(argc, argv, opts_short, options, &idx);
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			if (guid) {
				printf("Image type already specified\n");
				return -1;
			}
			guid = &efi_guid_image_type_uboot_fit;
			break;
		case 'r':
			if (guid) {
				printf("Image type already specified\n");
				return -1;
			}
			guid = &efi_guid_image_type_uboot_raw;
			break;
		case 'g':
			if (guid) {
				printf("Image type already specified\n");
				return -1;
			}
			if (uuid_parse(optarg, uuid_buf)) {
				printf("Wrong guid format\n");
				return -1;
			}
			convert_uuid_to_guid(uuid_buf);
			guid = (efi_guid_t *)uuid_buf;
			break;
		case 'i':
			index = strtoul(optarg, NULL, 0);
			break;
		case 'I':
			instance = strtoul(optarg, NULL, 0);
			break;
#ifdef CONFIG_TOOLS_LIBCRYPTO
		case 'p':
			if (privkey_file) {
				printf("Private Key already specified\n");
				return -1;
			}
			privkey_file = optarg;
			break;
		case 'c':
			if (cert_file) {
				printf("Certificate file already specified\n");
				return -1;
			}
			cert_file = optarg;
			break;
		case 'm':
			mcount = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			dump_sig = 1;
			break;
#endif /* CONFIG_TOOLS_LIBCRYPTO */
		case 'h':
			print_usage();
			return 0;
		}
	}

	/* check necessary parameters */
	if ((argc != optind + 2) || !guid ||
	    ((privkey_file && !cert_file) ||
	     (!privkey_file && cert_file))) {
		print_usage();
		return -1;
	}

	if (create_fwbin(argv[argc - 1], argv[argc - 2], guid, index, instance,
			 mcount, privkey_file, cert_file) < 0) {
		printf("Creating firmware capsule failed\n");
		return -1;
	}

	return 0;
}
