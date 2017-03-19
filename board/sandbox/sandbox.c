/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <cros_ec.h>
#include <dm.h>
#include <os.h>
#include <asm/state.h>
#include <asm/test.h>
#include <asm/u-boot-sandbox.h>

/*
 * Pointer to initial global data area
 *
 * Here we initialize it.
 */
gd_t *gd;

/* Add a simple GPIO device */
U_BOOT_DEVICE(gpio_sandbox) = {
	.name = "gpio_sandbox",
};

void flush_cache(unsigned long start, unsigned long size)
{
}

#ifndef CONFIG_TIMER
/* system timer offset in ms */
static unsigned long sandbox_timer_offset;

void sandbox_timer_add_offset(unsigned long offset)
{
	sandbox_timer_offset += offset;
}

unsigned long timer_read_counter(void)
{
	return os_get_nsec() / 1000 + sandbox_timer_offset * 1000;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	if (cros_ec_get_error()) {
		/* Force console on */
		gd->flags &= ~GD_FLG_SILENT;

		printf("cros-ec communications failure %d\n",
		       cros_ec_get_error());
		puts("\nPlease reset with Power+Refresh\n\n");
		panic("Cannot init cros-ec device");
		return -1;
	}
	return 0;
}
#endif

static int sandbox_phase(struct udevice *dev, enum board_phase_t phase)
{
	struct sandbox_state *state = state_get_current();

	switch (phase) {
	case BOARD_F_DRAM_INIT:
		gd->ram_size = state->ram_size;
		return 0;
	default:
		return -ENOSYS;
	}

	return 0;
}

static int sandbox_board_probe(struct udevice *dev)
{
	return board_support_phase(dev, BOARD_F_DRAM_INIT);
}

static const struct board_ops sandbox_board_ops = {
	.phase		= sandbox_phase,
};

/* Name this starting with underscore so it will be called last */
U_BOOT_DRIVER(_sandbox_board_drv) = {
	.name		= "sandbox_board",
	.id		= UCLASS_BOARD,
	.ops		= &sandbox_board_ops,
	.probe		= sandbox_board_probe,
	.flags		= DM_FLAG_PRE_RELOC,
};

U_BOOT_DEVICE(sandbox_board) = {
	.name		= "sandbox_board",
};
