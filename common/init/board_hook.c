/*
 * Board driver interface
 *
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <board.h>

DECLARE_GLOBAL_DATA_PTR;

int board_hook_walk_phase_count(enum board_phase_t phase)
{
	struct board_hook *drv =
		ll_entry_start(struct board_hook, board_hook);
	const int n_ents = ll_entry_count(struct board_hook, board_hook);
	struct board_hook *entry;
	int count = 0;
	int ret;

	printf("start\n");
	for (entry = drv; entry != drv + n_ents; entry++) {
		printf("entry = %s\n", entry->name);
		if (!(entry->phase_mask & board_phase_mask(phase)))
			continue;
		ret = entry->hook();
		if (ret == 0) {
			count++;
		} else if (ret == BOARD_PHASE_CLAIMED) {
			count++;
#if CONFIG_IS_ENABLED(BOARD_HOOK_NAMES)
			debug("Phase %d claimed by '%s'\n", phase, entry->name);
#else
			debug("Phase %d claimed (name not available)\n", phase);
#endif
			break;
		} else if (ret != -ENOSYS) {
			gd->phase_count[phase] += count;
			return ret;
		}
	}

	gd->phase_count[phase] += count;

	return count;
}

int board_hook_walk_phase(enum board_phase_t phase)
{
	int count;
	int ret;

	ret = board_hook_walk_phase_count(phase);
	if (ret < 0)
		return ret;
	count = ret;
#if CONFIG_IS_ENABLED(BOARD_ENABLE)
	ret = board_walk_phase_count(phase, true);
	if (ret < 0)
		return ret;
	count += ret;
#endif
	if (!count) {
		printf("Unable to find driver for phase %d\n", phase);
		return -ENOSYS;
	}

	return 0;
}

int board_hook_walk_opt_phase(enum board_phase_t phase)
{
	int ret;

	ret = board_hook_walk_phase_count(phase);
	if (ret < 0)
		return ret;
#if CONFIG_IS_ENABLED(BOARD_ENABLE)
	ret = board_walk_phase_count(phase);
	if (ret < 0)
		return ret;
#endif

	return 0;
}
