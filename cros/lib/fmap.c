// SPDX-License-Identifier: GPL-2.0+
/*
 * Sets up the read-write vboot portion (which loads the kernel)
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <log.h>
#include <cros/cros_ofnode.h>
#include <cros/fmap.h>

/* Make a best-effort assessment if the given fmap is real */
int fmap_valid(const struct fmap *fmap)
{
	if (memcmp(fmap, FMAP_SIGNATURE, strlen(FMAP_SIGNATURE)) != 0)
		return -EPERM;

	return 0;
}

int fmap_parse(const struct fmap *in, struct cros_fmap *fmap)
{
	int ret;
	uint i;

	ret = fmap_valid(in);
	if (ret)
		return log_msg_ret("valid", ret);

	for (i = 0; i < in->nareas; i++) {
		const struct fmap_area *area = &in->areas[i];
		struct fmap_entry *entry = NULL;
		const char *name = (char *)area->name;

		if (!strcmp("GBB", name))
			entry = &fmap->readonly.gbb;

		if (entry) {
			entry->offset = area->offset;
			entry->length = area->size;
		}
	}

	return 0;
}
