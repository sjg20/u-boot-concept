// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018, Tuomas Tynkkynen <tuomas.tynkkynen@iki.fi>
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <virtio.h>
#include <dm/device.h>
#include <dm/lists.h>

static const char *const virtio_drv_name[VIRTIO_ID_MAX_NUM] = {
	[VIRTIO_ID_NET]		= VIRTIO_NET_DRV_NAME,
	[VIRTIO_ID_BLOCK]	= VIRTIO_BLK_DRV_NAME,
};

int virtio_get_config(struct udevice *vdev, unsigned int offset,
		      void *buf, unsigned int len)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->get_config)
		return -ENOSYS;
	return ops->get_config(vdev->parent, offset, buf, len);
}

int virtio_set_config(struct udevice *vdev, unsigned int offset,
		      void *buf, unsigned int len)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->set_config)
		return -ENOSYS;
	return ops->set_config(vdev->parent, offset, buf, len);
}

int virtio_generation(struct udevice *vdev, u32 *counter)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->generation)
		return -ENOSYS;
	return ops->generation(vdev->parent, counter);
}

int virtio_get_status(struct udevice *vdev, u8 *status)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->get_status)
		return -ENOSYS;
	return ops->get_status(vdev->parent, status);
}

int virtio_set_status(struct udevice *vdev, u8 status)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->set_status)
		return -ENOSYS;
	return ops->set_status(vdev->parent, status);
}

int virtio_reset(struct udevice *vdev)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->reset)
		return -ENOSYS;
	return ops->reset(vdev->parent);
}

int virtio_get_features(struct udevice *vdev, u64 *features)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->get_features)
		return -ENOSYS;
	return ops->get_features(vdev->parent, features);
}

int virtio_set_features(struct udevice *vdev)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->set_features)
		return -ENOSYS;
	return ops->set_features(vdev->parent);
}

int virtio_find_vqs(struct udevice *vdev, unsigned int nvqs,
		    struct virtqueue *vqs[])
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->find_vqs)
		return -ENOSYS;
	return ops->find_vqs(vdev->parent, nvqs, vqs);
}

int virtio_del_vqs(struct udevice *vdev)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->del_vqs)
		return -ENOSYS;
	return ops->del_vqs(vdev->parent);
}

int virtio_notify(struct udevice *vdev, struct virtqueue *vq)
{
	struct dm_virtio_ops *ops;

	ops = virtio_get_ops(vdev->parent);
	if (!ops->notify)
		return -ENOSYS;
	return ops->notify(vdev->parent, vq);
}

void virtio_add_status(struct udevice *vdev, u8 status)
{
	u8 old;

	if (!virtio_get_status(vdev, &old))
		virtio_set_status(vdev, old | status);
}

int virtio_finalize_features(struct udevice *vdev)
{
	struct virtio_dev_priv *uc_priv = dev_get_uclass_priv(vdev->parent);
	u8 status;
	int ret;

	ret = virtio_set_features(vdev);
	if (ret)
		return ret;

	if (uc_priv->legacy)
		return 0;

	virtio_add_status(vdev, VIRTIO_CONFIG_S_FEATURES_OK);
	ret = virtio_get_status(vdev, &status);
	if (ret)
		return ret;
	if (!(status & VIRTIO_CONFIG_S_FEATURES_OK)) {
		debug("(%s): device refuses features %x\n", vdev->name, status);
		return -ENODEV;
	}

	return 0;
}

void virtio_driver_features_init(struct virtio_dev_priv *priv,
				 u32 *feature, u32 feature_size,
				 u32 *feature_legacy, u32 feature_legacy_size)
{
	priv->feature_table = feature;
	priv->feature_table_size = feature_size;
	priv->feature_table_legacy = feature_legacy;
	priv->feature_table_size_legacy = feature_legacy_size;
}

void virtio_init(void)
{
	struct udevice *bus;

	/* Enumerate all known virtio devices */
	for (uclass_first_device(UCLASS_VIRTIO, &bus);
	     bus;
	     uclass_next_device(&bus)) {
		;
	}
}

static int virtio_uclass_post_probe(struct udevice *udev)
{
	struct virtio_dev_priv *uc_priv = dev_get_uclass_priv(udev);
	char dev_name[30], *str;
	struct udevice *vdev;
	int ret;

	if (uc_priv->device > VIRTIO_ID_MAX_NUM) {
		debug("(%s): virtio device ID %d exceeds maximum num\n",
		      udev->name, uc_priv->device);
		return 0;
	}

	if (!virtio_drv_name[uc_priv->device]) {
		debug("(%s): underlying virtio device driver unavailable\n",
		      udev->name);
		return 0;
	}

	snprintf(dev_name, sizeof(dev_name), "%s#%d",
		 virtio_drv_name[uc_priv->device], udev->seq);
	str = strdup(dev_name);
	if (!str)
		return -ENOMEM;

	ret = device_bind_driver(udev, virtio_drv_name[uc_priv->device],
				 str, &vdev);
	if (ret == -ENOENT) {
		debug("(%s): no driver configured\n", udev->name);
		return 0;
	}
	if (ret) {
		free(str);
		return ret;
	}
	device_set_name_alloced(vdev);

	return 0;
}

static int virtio_uclass_child_post_bind(struct udevice *vdev)
{
	/* Acknowledge that we've seen the device */
	virtio_add_status(vdev, VIRTIO_CONFIG_S_ACKNOWLEDGE);

	return 0;
}

static int virtio_uclass_child_pre_probe(struct udevice *vdev)
{
	struct virtio_dev_priv *uc_priv = dev_get_uclass_priv(vdev->parent);
	u64 device_features;
	u64 driver_features;
	u64 driver_features_legacy;
	int i;
	int ret;

	/*
	 * Save the real virtio device (eg: virtio-net, virtio-blk) to
	 * the transport (parent) device's uclass priv for future use.
	 */
	uc_priv->vdev = vdev;

	/*
	 * We always start by resetting the device, in case a previous driver
	 * messed it up. This also tests that code path a little.
	 */
	ret = virtio_reset(vdev);
	if (ret)
		goto err;

	/* We have a driver! */
	virtio_add_status(vdev, VIRTIO_CONFIG_S_DRIVER);

	/* Figure out what features the device supports */
	virtio_get_features(vdev, &device_features);
	debug("(%s) plain device features supported %016llx\n",
	      vdev->name, device_features);
	if (!(device_features & (1ULL << VIRTIO_F_VERSION_1)))
		uc_priv->legacy = true;

	/* Figure out what features the driver supports */
	driver_features = 0;
	for (i = 0; i < uc_priv->feature_table_size; i++) {
		unsigned int f = uc_priv->feature_table[i];

		WARN_ON(f >= 64);
		driver_features |= (1ULL << f);
	}

	/* Some drivers have a separate feature table for virtio v1.0 */
	if (uc_priv->feature_table_legacy) {
		driver_features_legacy = 0;
		for (i = 0; i < uc_priv->feature_table_size_legacy; i++) {
			unsigned int f = uc_priv->feature_table_legacy[i];

			WARN_ON(f >= 64);
			driver_features_legacy |= (1ULL << f);
		}
	} else {
		driver_features_legacy = driver_features;
	}

	if (uc_priv->legacy) {
		debug("(%s): legacy virtio device\n", vdev->name);
		uc_priv->features = driver_features_legacy & device_features;
	} else {
		debug("(%s): v1.0 complaint virtio device\n", vdev->name);
		uc_priv->features = driver_features & device_features;
	}

	/* Transport features always preserved to pass to finalize_features */
	for (i = VIRTIO_TRANSPORT_F_START; i < VIRTIO_TRANSPORT_F_END; i++)
		if ((device_features & (1ULL << i)) &&
		    (i == VIRTIO_F_VERSION_1))
			__virtio_set_bit(vdev->parent, i);

	debug("(%s) final negotiated features supported %016llx\n",
	      vdev->name, uc_priv->features);
	ret = virtio_finalize_features(vdev);
	if (ret)
		goto err;

	return 0;

err:
	virtio_add_status(vdev, VIRTIO_CONFIG_S_FAILED);
	return ret;
}

static int virtio_uclass_child_post_probe(struct udevice *vdev)
{
	/* Indicates that the driver is set up and ready to drive the device */
	virtio_add_status(vdev, VIRTIO_CONFIG_S_DRIVER_OK);

	return 0;
}

UCLASS_DRIVER(virtio) = {
	.name	= "virtio",
	.id	= UCLASS_VIRTIO,
	.flags	= DM_UC_FLAG_SEQ_ALIAS,
	.post_probe = virtio_uclass_post_probe,
	.child_post_bind = virtio_uclass_child_post_bind,
	.child_pre_probe = virtio_uclass_child_pre_probe,
	.child_post_probe = virtio_uclass_child_post_probe,
	.per_device_auto_alloc_size = sizeof(struct virtio_dev_priv),
};
