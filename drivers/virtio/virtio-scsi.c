/*
 * U-Boot Driver for VIRTIO SCSI Host
 *
 * (C) Copyright 2025, Your Name <your.email@example.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <pci.h>
#include <scsi.h>
#include <virtio.h>
#include <virtio_pci.h>
#include <linux/virtio_scsi.h>

/* Private data structure for our driver instance */
struct virtio_scsi_priv {
	struct virtio_device *vdev;
	struct virtqueue *c_vq; /* Control virtqueue */
	struct virtqueue *r_vq; /* Request virtqueue */

	/* We need a pre-allocated request/response area */
	struct virtio_scsi_req_cmd req;
	struct virtio_scsi_resp_cmd resp;
};

/**
 * virtio_scsi_exec_cmd() - Executes a SCSI command
 * @dev: The udevice instance for our driver
 * @cdb: The Command Descriptor Block to execute
 * @cdb_len: Length of the CDB
 * @data: Pointer to the data buffer for read/write
 * @data_len: Length of the data buffer
 * @return: 0 on success, error code otherwise
 */
static int virtio_scsi_exec_cmd(struct udevice *dev, u8 *cdb, u8 cdb_len,
				void *data, uint data_len)
{
	struct virtio_scsi_priv *priv = dev_get_priv(dev);
	struct virtqueue *vq = priv->r_vq;
	struct vring_desc *desc;
	int ret, len, head;
	u16 out_num, in_num;

	debug("virtio-scsi: Executing CDB[0]=0x%02x, data_len=%u\n", cdb[0], data_len);

	/*
	 * TODO: Implement proper request tracking if multiple commands
	 * need to be in-flight. For U-Boot's synchronous model, one is enough.
	 */

	/* 1. Prepare the request header */
	priv->req.lun[0] = 1; /* LUN 0 */
	priv->req.lun[1] = 0; /* TODO: This is a simplified LUN mapping */
	priv->req.tag = 0;    /* Simple tag for a single request */
	memcpy(priv->req.cdb, cdb, cdb_len);
	memset(priv->req.cdb + cdb_len, 0, sizeof(priv->req.cdb) - cdb_len);

	/* 2. Set up descriptors for the virtqueue */
	out_num = 0;
	in_num = 0;
	head = -1;

	/* Descriptor for the request header (OUT) */
	ret = virtqueue_add_buf(vq, &priv->req, sizeof(priv->req),
				VRING_DESC_F_NEXT, &head);
	if (ret < 0)
		return ret;
	out_num++;

	/* Descriptor for the data buffer (OUT for WRITE, IN for READ) */
	if (data_len > 0) {
		if (cdb[0] == SCSI_CMD_WRITE_10 || cdb[0] == SCSI_CMD_WRITE_6) {
			ret = virtqueue_add_buf(vq, data, data_len,
						VRING_DESC_F_NEXT, &head);
			if (ret < 0)
				return ret;
			out_num++;
		}
	}

	/* Descriptor for the response header (IN) */
	ret = virtqueue_add_buf(vq, &priv->resp, sizeof(priv->resp),
				VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, &head);
	if (ret < 0)
		return ret;
	in_num++;

	/* Descriptor for the data buffer (IN for READ) */
	if (data_len > 0) {
		if (cdb[0] == SCSI_CMD_INQUIRY || cdb[0] == SCSI_CMD_READ_10) {
			ret = virtqueue_add_buf(vq, data, data_len,
						VRING_DESC_F_WRITE, &head);
			if (ret < 0)
				return ret;
			in_num++;
		}
	}

	/* 3. Submit the request to the host */
	ret = virtqueue_kick(vq, head);
	if (ret < 0)
		return ret;

	/* 4. Poll for the response */
	/* TODO: Add a timeout mechanism */
	while (!virtqueue_get_buf(vq, &len)) {
		/* Wait */
		WATCHDOG_RESET();
		udelay(100);
	}

	/* 5. Check the response */
	if (priv->resp.response != VIRTIO_SCSI_S_OK) {
		printf("virtio-scsi: Command failed with response 0x%x, status 0x%x\n",
		       priv->resp.response, priv->resp.status);
		return -EIO;
	}

	debug("virtio-scsi: Command successful\n");

	return 0;
}

static int virtio_scsi_probe(struct udevice *dev)
{
	struct virtio_scsi_priv *priv = dev_get_priv(dev);
	struct virtio_device *vdev;
	int ret;

	printf("Probing VIRTIO-SCSI device\n");

	/* Get the virtio_device created by the PCI bus driver */
	vdev = pci_get_virtio_device(dev);
	if (!vdev) {
		printf("Error: Failed to get virtio device from PCI.\n");
		return -ENODEV;
	}
	priv->vdev = vdev;

	/*
	 * VIRTIO Initialization Sequence (as per spec)
	 */

	/* 1. Reset device */
	virtio_reset_device(vdev);

	/* 2. Acknowledge and set DRIVER status */
	virtio_set_status(vdev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

	/*
	 * 3. Negotiate features.
	 * TODO: Read device features and write out the subset we support.
	 * For now, we support no extra features.
	 */
	u64 features = 0;
	ret = virtio_negotiate_features(vdev, features);
	if (ret) {
		printf("Error: Feature negotiation failed\n");
		return ret;
	}

	/* 4. Find our virtqueues */
	/* We need at least the first request queue and the control queue */
	priv->c_vq = virtio_find_vq(vdev, 0, "control");
	if (!priv->c_vq) {
		printf("Error: Failed to find control virtqueue\n");
		return -ENODEV;
	}

	priv->r_vq = virtio_find_vq(vdev, 2, "request");
	if (!priv->r_vq) {
		printf("Error: Failed to find request virtqueue\n");
		return -ENODEV;
	}

	/* 5. Mark driver as ready */
	virtio_set_status(vdev, VIRTIO_CONFIG_S_DRIVER_OK);

	printf("VIRTIO-SCSI probe successful\n");

	return 0;
}

/* Define the operations for the scsi uclass */
static const struct scsi_ops virtio_scsi_ops = {
	.exec_cmd = virtio_scsi_exec_cmd,
	/* .scan is optional and can be handled by the uclass */
};

/* Define compatible strings to match against the device tree */
static const struct udevice_id virtio_scsi_ids[] = {
	{ .compatible = "virtio,pci-scsi" },
	{ }
};

U_BOOT_DRIVER(virtio_scsi) = {
	.name		= "virtio_scsi",
	.id		= UCLASS_SCSI,
	.of_match	= virtio_scsi_ids,
	.priv_auto_alloc_size = sizeof(struct virtio_scsi_priv),
	.probe		= virtio_scsi_probe,
	.ops		= &virtio_scsi_ops,
	.flags		= DM_FLAG_PROBE_AFTER_BIND,
};
