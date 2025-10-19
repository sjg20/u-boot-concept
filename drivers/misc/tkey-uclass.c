// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Canonical Ltd
 *
 * Tillitis TKey security token uclass
 */

#define LOG_CATEGORY	UCLASS_TKEY

#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <string.h>
#include <tkey.h>
#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <u-boot/blake2.h>
#include <u-boot/schedule.h>

/* TKey Protocol Constants */
#define TKEY_FRAME_HEADER_SIZE		1
#define TKEY_MAX_DATA_SIZE		128
#define TKEY_MAX_FRAME_SIZE		(TKEY_FRAME_HEADER_SIZE + \
					 TKEY_MAX_DATA_SIZE)

/* Frame Header Bits */
#define TKEY_FRAME_ID_MASK		0x60
#define TKEY_FRAME_ENDPOINT_MASK	0x18
#define TKEY_FRAME_STATUS_MASK		0x04
#define TKEY_FRAME_LEN_MASK		0x03

/* Frame ID Values */
#define TKEY_FRAME_ID_CMD		0
#define TKEY_FRAME_ID_RSP		1

/* Endpoint Values */
#define TKEY_ENDPOINT_FIRMWARE		2
#define TKEY_ENDPOINT_APP		3

/* Data Length Values */
#define TKEY_LEN_1_BYTE			0
#define TKEY_LEN_4_BYTES		1
#define TKEY_LEN_32_BYTES		2
#define TKEY_LEN_128_BYTES		3

/* Status Values */
#define TKEY_STATUS_OK			0
#define TKEY_STATUS_ERROR		0x04

/* Firmware Commands */
#define TKEY_FW_CMD_NAME_VERSION	0x01
#define TKEY_FW_CMD_LOAD_APP		0x03
#define TKEY_FW_CMD_LOAD_APP_DATA	0x05
#define TKEY_FW_CMD_GET_UDI		0x08

/* Signer App Commands */
#define TKEY_APP_CMD_GET_PUBKEY		0x01
#define TKEY_APP_RSP_GET_PUBKEY		0x02

/* Constants */
#define TKEY_USS_SIZE			32
#define TKEY_PUBKEY_SIZE		32

/* Timeouts (ms) */
#define TKEY_TIMEOUT_MS			1000
#define TKEY_LOAD_TIMEOUT_MS		2000

/* TKey frame structure */
struct tkey_frame {
	u8 header;
	u8 data[TKEY_MAX_DATA_SIZE];
};

/**
 * make_hdr() - Build a TKey frame header byte
 *
 * Constructs a TKey protocol frame header by encoding the frame ID, endpoint,
 * status, and length code into a single byte according to the TKey protocol
 * specification.
 *
 * Frame header format (8 bits):
 *   Bit  7:   Reserved (always 0)
 *   Bits 6-5: Frame ID (0=CMD, 1=RSP, 2-3=reserved)
 *   Bits 4-3: Endpoint (0-1=reserved, 2=Firmware, 3=App)
 *   Bit  2:   Status (0=OK, 1=Error)
 *   Bits 1-0: Length code (0=1 byte, 1=4 bytes, 2=32 bytes, 3=128 bytes)
 *
 * @id: Frame ID (TKEY_FRAME_ID_CMD=0 or TKEY_FRAME_ID_RSP=1)
 * @endpoint: Target endpoint (TKEY_ENDPOINT_FIRMWARE=2 or TKEY_ENDPOINT_APP=3)
 * @status: Status flag (TKEY_STATUS_OK=0 or TKEY_STATUS_ERROR=1)
 * @len: Length code (TKEY_LEN_1_BYTE=0, TKEY_LEN_4_BYTES=1,
 *                    TKEY_LEN_32_BYTES=2, TKEY_LEN_128_BYTES=3)
 * Return: 8-bit frame header value
 */
static uint make_hdr(uint id, uint endpoint, uint status, uint len)
{
	return ((id & 0x3) << 5) | ((endpoint & 0x3) << 3) |
	       ((status & 0x1) << 2) | (len & 0x3);
}

static int tkey_send_frame(struct udevice *dev,
			   const struct tkey_frame *frame, int len)
{
	u8 buffer[TKEY_MAX_FRAME_SIZE];
	int total_len = TKEY_FRAME_HEADER_SIZE + len;
	int ret;

	log_debug("Sending frame - header=%02x, len=%x\n", frame->header, len);

	/* Build frame */
	buffer[0] = frame->header;
	if (len > 0)
		memcpy(&buffer[1], frame->data, len);

	/* Send via generic write */
	ret = tkey_write(dev, buffer, total_len);
	if (ret < 0) {
		log_debug("Frame send failed\n");
		return ret;
	}
	log_debug("Frame sent successfully\n");

	return total_len;
}

static int tkey_recv_frame(struct udevice *dev, struct tkey_frame *frame,
			   int timeout_ms)
{
	const struct tkey_ops *ops;
	int len, ret;
	u8 buf[256];

	log_debug("Receiving frame...\n");
	ops = tkey_get_ops(dev);

	/* Try read_all first for USB devices that send raw responses */
	if (ops->read_all) {
		log_debug("Using read_all for USB raw response reception\n");
		ret = tkey_read_all(dev, buf, sizeof(buf), timeout_ms);
		if (ret < 0) {
			log_debug("Read_all failed: %d\n", ret);
			return ret;
		}
		if (ret < 1) {
			log_debug("Read_all got no data\n");
			return -EIO;
		}
		log_debug("USB raw response: %x bytes received\n", ret);

		/*
		 * USB TKey sends raw responses, not framed responses. Create a
		 * synthetic frame with a success header
		 */
		frame->header = make_hdr(TKEY_FRAME_ID_RSP,
					 TKEY_ENDPOINT_FIRMWARE,
					 TKEY_STATUS_OK, TKEY_LEN_128_BYTES);

		/* Copy the raw response data */
		len = 0;
		if (ret > 0) {
			len = min(ret, (int)TKEY_MAX_DATA_SIZE);
			memcpy(frame->data, buf, len);
		}

		log_debug("USB raw response converted to frame: header=%02x, "
			  "data_len=%x\n", frame->header, len);
		return TKEY_FRAME_HEADER_SIZE + len;
	}

	/* Fallback to byte-by-byte reading for serial devices */
	log_debug("Using byte-by-byte frame reception\n");

	/* Read header first */
	ret = tkey_read(dev, &frame->header, 1, timeout_ms);
	if (ret != 1) {
		log_debug("Header read failed: got %x bytes\n", ret);
		return (ret < 0) ? ret : -EIO;
	}

	log_debug("Received header: %02x\n", frame->header);

	/* Decode data length from header */
	switch (frame->header & TKEY_FRAME_LEN_MASK) {
	case TKEY_LEN_1_BYTE:
		len = 1;
		break;
	case TKEY_LEN_4_BYTES:
		len = 4;
		break;
	case TKEY_LEN_32_BYTES:
		len = 32;
		break;
	case TKEY_LEN_128_BYTES:
		len = 128;
		break;
	default:
		log_debug("Invalid length code: %02x\n",
			  frame->header & TKEY_FRAME_LEN_MASK);
		return -EINVAL;
	}

	log_debug("Expected data length: %x bytes\n", len);

	/* Read data bytes if any */
	if (len > 0) {
		ret = tkey_read(dev, frame->data, len, timeout_ms);
		if (ret != len) {
			log_debug("Data read failed: expected %x, got %x "
				  "bytes\n", len, ret);
			return (ret < 0) ? ret : -EIO;
		}
	}

	log_debug("got frame: %x total bytes\n", TKEY_FRAME_HEADER_SIZE + len);

	return TKEY_FRAME_HEADER_SIZE + len;
}

int tkey_read(struct udevice *dev, void *buf, int len, int timeout_ms)
{
	const struct tkey_ops *ops = tkey_get_ops(dev);

	return ops->read(dev, buf, len, timeout_ms);
}

int tkey_write(struct udevice *dev, const void *buf, int len)
{
	const struct tkey_ops *ops = tkey_get_ops(dev);

	return ops->write(dev, buf, len);
}

int tkey_read_all(struct udevice *dev, void *buf, int maxlen, int timeout_ms)
{
	const struct tkey_ops *ops = tkey_get_ops(dev);

	/* Use read_all if available, otherwise fall back to regular read */
	if (ops->read_all)
		return ops->read_all(dev, buf, maxlen, timeout_ms);

	return ops->read(dev, buf, maxlen, timeout_ms);
}

int tkey_get_udi(struct udevice *dev, void *udi)
{
	struct tkey_frame cmd_frame, rsp_frame;
	int ret;

	/* Build command frame */
	cmd_frame.header = make_hdr(TKEY_FRAME_ID_CMD, TKEY_ENDPOINT_FIRMWARE,
				    0, TKEY_LEN_1_BYTE);
	cmd_frame.data[0] = TKEY_FW_CMD_GET_UDI;

	/* Send command */
	ret = tkey_send_frame(dev, &cmd_frame, 1);
	if (ret < 0)
		return ret;

	/* Receive response */
	ret = tkey_recv_frame(dev, &rsp_frame, TKEY_TIMEOUT_MS);
	if (ret < 0)
		return ret;

	/* Check response status */
	if (rsp_frame.header & TKEY_FRAME_STATUS_MASK) {
		/* GetUDI is a firmware command - check if we're in app mode */
		char name0[TKEY_NAME_SIZE], name1[TKEY_NAME_SIZE];
		u32 version;

		if (!tkey_get_name_version(dev, name0, name1, &version)) {
			if (!strcmp(name0, "tk1 ") && !strcmp(name1, "sign")) {
				log_debug("GetUDI failed - device is in app mode, UDI only available in firmware mode\n");
				return -ENOTSUPP;
			}
		}

		log_debug("GetUDI failed with error status, error code=%02x\n",
			  ret > 1 ? rsp_frame.data[0] : 0);
		return -EIO;
	}

	/* Extract UDI */
	if (ret >= (TKEY_FRAME_HEADER_SIZE + TKEY_UDI_SIZE)) {
		/*
		 * For USB responses, check if we have the expected response
		 * pattern. USB TKey UDI responses have format: [padding...]
		 * [0x52] [0x09] [status] [UDI...]
		 */
		int ofs = -1;

		/* Look for the USB response pattern 0x52 0x09 */
		for (int i = 0; i < ret - TKEY_UDI_SIZE - 3; i++) {
			if (rsp_frame.data[i] == 0x52 &&
			    rsp_frame.data[i + 1] == 0x09) {
				ofs = i;
				break;
			}
		}

		if (ofs >= 0) {
			/* USB format: UDI starts after 0x52 0x09 status */
			memcpy(udi, &rsp_frame.data[ofs + 3], TKEY_UDI_SIZE);
		} else {
			/* Standard format: UDI starts at offset 0 */
			memcpy(udi, rsp_frame.data, TKEY_UDI_SIZE);
		}
		return 0;
	}

	return -EINVAL;
}

int tkey_get_name_version(struct udevice *dev, char name0[TKEY_NAME_SIZE],
			  char name1[TKEY_NAME_SIZE], u32 *version)
{
	struct tkey_frame cmd_frame, rsp_frame;
	int ret;

	/* Build command frame */
	cmd_frame.header = make_hdr(TKEY_FRAME_ID_CMD, TKEY_ENDPOINT_FIRMWARE,
				    0, TKEY_LEN_1_BYTE);
	cmd_frame.data[0] = TKEY_FW_CMD_NAME_VERSION;

	/* Send command */
	ret = tkey_send_frame(dev, &cmd_frame, 1);
	if (ret < 0)
		return ret;

	/* Receive response */
	ret = tkey_recv_frame(dev, &rsp_frame, TKEY_TIMEOUT_MS);
	if (ret < 0)
		return ret;

	/* Check response status and handle different modes */
	if (rsp_frame.header & TKEY_FRAME_STATUS_MASK) {
		/*
		 * Error status set - could be app mode responding to
		 * firmware command
		 */
		log_debug("GetNameVersion status bit set, header=%02x, "
			  "error code=%02x\n", rsp_frame.header,
			  ret > 1 ? rsp_frame.data[0] : 0);

		/*
		 * In app mode, TKey responds with error status to firmware
		 * commands. Try to decode as app mode response
		 */
		if (ret >= 1 && rsp_frame.data[0] == 0x00) {
			/* App mode: return standard app identifiers */
			strcpy(name0, "tk1 ");
			strcpy(name1, "sign");
			*version = 1;  /* Default app version */
			log_debug("Detected app mode response, using default "
				  "app identifiers\n");
			return 0;
		}
		return -EIO;
	}

	/* Parse response data */
	if (ret >= 13) { /* Header + 4 + 4 + 4 bytes */
		/*
		 * For USB responses, check if we have the expected response
		 * pattern. USB TKey responses have format: [padding...] [0x52]
		 * [0x02] [tk1 ] [mkdf] [version]
		 */
		int ofs = -1;

		/* Look for the USB response pattern 0x52 0x02 */
		for (int i = 0; i < ret - 13; i++) {
			if (rsp_frame.data[i] == 0x52 &&
			    rsp_frame.data[i + 1] == 0x02) {
				ofs = i;
				break;
			}
		}

		if (ofs >= 0) {
			/* USB format: found pattern at offset */
			memcpy(name0, &rsp_frame.data[ofs + 2], 4);
			name0[4] = '\0';
			memcpy(name1, &rsp_frame.data[ofs + 6], 4);
			name1[4] = '\0';
			*version =
				get_unaligned_le32(&rsp_frame.data[ofs + 10]);
		} else {
			/*
			 * Standard format: skip the first byte (command
			 * response code)
			 */
			memcpy(name0, &rsp_frame.data[1], 4);
			name0[4] = '\0';
			memcpy(name1, &rsp_frame.data[5], 4);
			name1[4] = '\0';
			*version = get_unaligned_le32(&rsp_frame.data[9]);
		}
	}

	return 0;
}

int tkey_in_app_mode(struct udevice *dev)
{
	char name0[TKEY_NAME_SIZE], name1[TKEY_NAME_SIZE];
	u32 version;
	int ret;

	ret = tkey_get_name_version(dev, name0, name1, &version);
	if (ret)
		return ret;

	/* Check if in firmware mode */
	if (!strcmp(name0, "tk1 ") && !strcmp(name1, "mkdf"))
		return 0;  /* Firmware mode */

	/* Check if in app mode */
	if (!strcmp(name0, "tk1 ") && !strcmp(name1, "sign"))
		return 1;  /* App mode */

	/* Unknown mode */
	return -EINVAL;
}

static int tkey_load_app_header(struct udevice *dev, int app_size,
				const void *uss, int uss_size)
{
	struct tkey_frame cmd_frame, rsp_frame;
	int ret;

	log_debug("Loading app header, size=%u\n", app_size);

	/*
	 * Build LOAD_APP command frame with app size (128-byte frame like
	 * Go app)
	 */
	cmd_frame.header = make_hdr(TKEY_FRAME_ID_CMD, TKEY_ENDPOINT_FIRMWARE,
				    TKEY_STATUS_OK, TKEY_LEN_128_BYTES);
	cmd_frame.data[0] = TKEY_FW_CMD_LOAD_APP;
	/* Pack app size as little-endian 32-bit */
	cmd_frame.data[1] = app_size & 0xff;
	cmd_frame.data[2] = (app_size >> 8) & 0xff;
	cmd_frame.data[3] = (app_size >> 16) & 0xff;
	cmd_frame.data[4] = (app_size >> 24) & 0xff;

	/* Include USS if provided */
	if (uss && uss_size > 0) {
		blake2s_state state;
		u8 uss_hash[32];

		/* Hash the USS using BLAKE2s to get 32 bytes */
		ret = blake2s_init(&state, 32);
		if (ret) {
			log_debug("Failed to init BLAKE2s\n");
			return ret;
		}

		ret = blake2s_update(&state, uss, uss_size);
		if (ret) {
			log_debug("Failed to update BLAKE2s\n");
			return ret;
		}

		ret = blake2s_final(&state, uss_hash, 32);
		if (ret) {
			log_debug("Failed to finalize BLAKE2s\n");
			return ret;
		}

		/* USS present flag */
		cmd_frame.data[5] = 1;
		/* Copy USS hash (32 bytes) */
		memcpy(&cmd_frame.data[6], uss_hash, 32);
		/* Pad remaining bytes with zeros */
		memset(&cmd_frame.data[38], '\0', 128 - 38);

		log_debug("USS hash included in app header\n");
	} else {
		/* No USS - set flag to 0 and pad with zeros */
		memset(&cmd_frame.data[5], '\0', 128 - 5);
	}

	/* Send command */
	ret = tkey_send_frame(dev, &cmd_frame, 128);
	if (ret < 0)
		return ret;

	/* Receive response */
	ret = tkey_recv_frame(dev, &rsp_frame, TKEY_LOAD_TIMEOUT_MS);
	if (ret < 0)
		return ret;

	/* Check response status */
	if (rsp_frame.header & TKEY_STATUS_ERROR) {
		log_debug("Load app header failed with error status\n");
		return -EIO;
	}

	log_debug("App header loaded successfully\n");

	return 0;
}

static int tkey_load_app_data(struct udevice *dev, const void *data, int size)
{
	struct tkey_frame cmd_frame, rsp_frame;
	int offset = 0;
	int ret;

	log_debug("Loading app data, %u bytes\n", size);

	while (offset < size) {
		int todo = min(size - offset, TKEY_MAX_DATA_SIZE - 1);
		u8 len_code;

		/* Determine length code for chunk */
		if (todo <= 1)
			len_code = TKEY_LEN_1_BYTE;
		else if (todo <= 4)
			len_code = TKEY_LEN_4_BYTES;
		else if (todo <= 32)
			len_code = TKEY_LEN_32_BYTES;
		else
			len_code = TKEY_LEN_128_BYTES;

		/*
		 * Build LOAD_APP_DATA command (always use 128-byte frames
		 * like Go app)
		 */
		cmd_frame.header = make_hdr(TKEY_FRAME_ID_CMD,
					    TKEY_ENDPOINT_FIRMWARE,
					    TKEY_STATUS_OK,
					    TKEY_LEN_128_BYTES);
		cmd_frame.data[0] = TKEY_FW_CMD_LOAD_APP_DATA;
		memcpy(&cmd_frame.data[1], data + offset, todo);

		/* Pad remaining bytes with zeros */
		if (todo + 1 < 128)
			memset(&cmd_frame.data[todo + 1], '\0',
			       128 - (todo + 1));

		/* Send chunk (always 128 bytes like Go app) */
		ret = tkey_send_frame(dev, &cmd_frame, 128);
		if (ret < 0)
			return ret;

		/* Receive response */
		ret = tkey_recv_frame(dev, &rsp_frame, TKEY_LOAD_TIMEOUT_MS);
		if (ret < 0)
			return ret;

		/* Check response status */
		if (rsp_frame.header & TKEY_STATUS_ERROR) {
			log_debug("Load app data failed at offset %u\n",
				  offset);
			return -EIO;
		}

		offset += todo;
		log_debug("Loaded chunk: %u/%u bytes\n", offset, size);
		schedule();
	}

	log_debug("App data loaded successfully\n");

	return 0;
}

int tkey_load_app_with_uss(struct udevice *dev, const void *app_data,
			   int app_size, const void *uss, int uss_size)
{
	int ret;

	/* Check if we're in firmware mode first */
	ret = tkey_in_app_mode(dev);
	if (ret < 0) {
		log_debug("Failed to check device mode (error %d)\n", ret);
		return ret;
	}

	if (ret) {
		log_debug("Device must be in firmware mode to load app\n");
		return -ENOTSUPP;
	}

	log_debug("Loading app (%u bytes)...\n", app_size);

	/* Send app header with size and USS (if provided) */
	ret = tkey_load_app_header(dev, app_size, uss, uss_size);
	if (ret) {
		log_debug("Failed to send app header (error %d)\n", ret);
		return ret;
	}

	/* Send app data */
	ret = tkey_load_app_data(dev, app_data, app_size);
	if (ret) {
		log_debug("Failed to send app data (error %d)\n", ret);
		return ret;
	}

	log_debug("App loaded successfully\n");

	return 0;
}

int tkey_load_app(struct udevice *dev, const void *app_data, int app_size)
{
	return tkey_load_app_with_uss(dev, app_data, app_size, NULL, 0);
}

int tkey_get_pubkey(struct udevice *dev, void *pubkey)
{
	struct tkey_frame cmd_frame, rsp_frame;
	int ret;

	/* Build GET_PUBKEY command frame */
	cmd_frame.header = make_hdr(TKEY_FRAME_ID_CMD, TKEY_ENDPOINT_APP,
				    TKEY_STATUS_OK, TKEY_LEN_1_BYTE);
	cmd_frame.data[0] = TKEY_APP_CMD_GET_PUBKEY;

	log_debug("Getting public key from signer app\n");

	/* Send command */
	ret = tkey_send_frame(dev, &cmd_frame, 1);
	if (ret < 0)
		return ret;

	/* Receive response */
	ret = tkey_recv_frame(dev, &rsp_frame, TKEY_TIMEOUT_MS);
	if (ret < 0)
		return ret;

	/* Check response status */
	if (rsp_frame.header & TKEY_FRAME_STATUS_MASK) {
		log_debug("GetPubkey failed with error status\n");
		return -EIO;
	}

	/* Extract public key (32 bytes) from response */
	if (ret >= TKEY_FRAME_HEADER_SIZE + TKEY_PUBKEY_SIZE) {
		memcpy(pubkey, rsp_frame.data, TKEY_PUBKEY_SIZE);
		log_debug("Public key retrieved successfully\n");
		return 0;
	}
	log_debug("GetPubkey response too short: %d bytes\n", ret);

	return -EINVAL;
}

int tkey_derive_disk_key(struct udevice *dev, const void *app_data,
			 int app_size, const void *uss, int uss_size,
			 void *disk_key, void *pubkey, void *key_hash)
{
	int ret;

	/* Load the signer app with USS */
	log_debug("Loading signer app with USS for disk key derivation\n");
	ret = tkey_load_app_with_uss(dev, app_data, app_size, uss,
				     uss_size);
	if (ret == -ENOTSUPP) {
		/* Already in app mode - continue */
		log_debug("App already loaded, retrieving key\n");
	} else if (ret) {
		log_debug("Failed to load app (error %d)\n", ret);
		return ret;
	}

	/* Get public key from signer */
	ret = tkey_get_pubkey(dev, pubkey);
	if (ret) {
		log_debug("Failed to get public key (error %d)\n", ret);
		return ret;
	}

	log_debug("Public key retrieved\n");

	/* Derive disk encryption key from public key using BLAKE2b */
	ret = blake2b(disk_key, 32, pubkey, 32, NULL, 0);
	if (ret) {
		log_debug("Failed to derive disk key (error %d)\n", ret);
		return ret;
	}

	log_debug("Disk encryption key derived\n");

	/* Generate verification hash if requested */
	if (key_hash) {
		ret = blake2b(key_hash, 32, disk_key, 32, NULL, 0);
		if (ret) {
			log_debug("Failed to generate verification hash "
				  "(error %d)\n", ret);
			return ret;
		}
		log_debug("Verification hash generated\n");
	}

	return 0;
}

int tkey_derive_wrapping_key(struct udevice *dev, const char *password,
			     void *wrapping_key)
{
	u8 udi[TKEY_UDI_SIZE];
	blake2b_state state;
	int ret;

	/* Get UDI from device (only available in firmware mode) */
	ret = tkey_get_udi(dev, udi);
	if (ret) {
		log_debug("Failed to get UDI (error %d)\n", ret);
		return ret;
	}

	/* Derive wrapping key using BLAKE2b(UDI || password) */
	ret = blake2b_init(&state, TKEY_WRAPPING_KEY_SIZE);
	if (ret)
		return ret;

	ret = blake2b_update(&state, udi, TKEY_UDI_SIZE);
	if (ret)
		return ret;

	ret = blake2b_update(&state, password, strlen(password));
	if (ret)
		return ret;

	ret = blake2b_final(&state, wrapping_key, TKEY_WRAPPING_KEY_SIZE);
	if (ret)
		return ret;

	log_debug("Wrapping key derived from password and UDI\n");

	return 0;
}

UCLASS_DRIVER(tkey) = {
	.id		= UCLASS_TKEY,
	.name		= "tkey",
};
