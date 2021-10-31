/* SPDX-License-Identifier: BSD-3-Clause */

/* taken from https://github.com/tpm2-software/tpm2-tss/blob/master/include/tss2/tss2_tpm2_types.h */
#define TPM2_MAX_PCRS           32

/* Hash algorithm sizes */
#define TPM2_SHA_DIGEST_SIZE     20
#define TPM2_SHA1_DIGEST_SIZE    20
#define TPM2_SHA256_DIGEST_SIZE  32
#define TPM2_SHA384_DIGEST_SIZE  48
#define TPM2_SHA512_DIGEST_SIZE  64
#define TPM2_SM3_256_DIGEST_SIZE 32

/* taken from https://github.com/tpm2-software/tpm2-tools/blob/master/lib/tpm2_eventlog.h#L14 */

typedef bool (*digest2_callback)(void const *digest, size_t size, void *data);
typedef bool (*event2_callback)(void const *event_hdr, size_t size, void *data);
typedef bool (*event2data_callback)(void const *event, u32 type, void *data,
				    u32 eventlog_version);
typedef bool (*specid_callback)(void const *event, void *data);
typedef bool (*log_event_callback)(void const *event_hdr, size_t size,
				   void *data);

struct tpm2_eventlog_context {
	void *data;
	specid_callback specid_cb;
	log_event_callback log_eventhdr_cb;
	event2_callback event2hdr_cb;
	digest2_callback digest2_cb;
	event2data_callback event2_cb;
	u32 sha1_used;
	u32 sha256_used;
	u32 sha384_used;
	u32 sha512_used;
	u32 sm3_256_used;
	u8 sha1_pcrs[TPM2_MAX_PCRS][TPM2_SHA1_DIGEST_SIZE];
	u8 sha256_pcrs[TPM2_MAX_PCRS][TPM2_SHA256_DIGEST_SIZE];
	u8 sha384_pcrs[TPM2_MAX_PCRS][TPM2_SHA384_DIGEST_SIZE];
	u8 sha512_pcrs[TPM2_MAX_PCRS][TPM2_SHA512_DIGEST_SIZE];
	u8 sm3_256_pcrs[TPM2_MAX_PCRS][TPM2_SM3_256_DIGEST_SIZE];
	u32 eventlog_version;
};
