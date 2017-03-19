/*
 * Copyright (c) 2017 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <board.h>
#include <dm.h>
#include <asm/state.h>
#include <asm/test.h>

static int board_sandbox_phase(struct udevice *dev, enum board_phase_t phase)
{
	struct sandbox_state *state = state_get_current();
	int id = dev_get_driver_data(dev);

	return state->board_sandbox_ret[id];
}

static int board_sandbox_probe(struct udevice *dev)
{
	return board_support_phase(dev, BOARD_PHASE_TEST);
}

static const struct board_ops board_sandbox_ops = {
	.phase		= board_sandbox_phase,
};


static const struct udevice_id board_sandbox_ids[] = {
	{ .compatible = "sandbox,board-test0", BOARD_TEST0 },
	{ .compatible = "sandbox,board-test1", BOARD_TEST1 },
	{ .compatible = "sandbox,board-test2", BOARD_TEST2 },
	{ }
};

U_BOOT_DRIVER(board_sandbox_drv) = {
	.name		= "board_sandbox",
	.id		= UCLASS_BOARD,
	.ops		= &board_sandbox_ops,
	.of_match	= board_sandbox_ids,
	.probe		= board_sandbox_probe,
};
