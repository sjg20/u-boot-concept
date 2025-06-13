// SPDX-License-Identifier: GPL-2.0
/*
 * Implementation of files on a filesystem
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <file.h>

UCLASS_DRIVER(file) = {
	.name	= "file",
	.id	= UCLASS_FILE,
	.per_device_auto	= sizeof(struct file_priv),
};
