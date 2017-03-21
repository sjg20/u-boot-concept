/*
 * Board driver interface
 *
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __BOARD_H
#define __BOARD_H

/* Include dm.h here because board.h comes before dm.h in include order */
#include <dm.h>

/*
 * This file provides access to board drivers, which are responsible for
 * initing the board as well as (in future) querying its state.
 */

/* Phases of init - please update phase_name also */
enum board_phase_t {
	/*
	 * Pre-relocation phases.
	 * TODO(sjg@chromium.org): At present these are named the same as the
	 * functions they replace to avoid confusion. However this naming is
	 * not very consistent. At some point once enough boards uses this
	 * interface, we could rename some of these.
	 *
	 * TODO(sjg@chromium.org): arch_cpu_init() and mach_cpu_init() are
	 * called before driver model is ready so we cannot yet add them to
	 * this interface. Hopefully this can be adjusted later:
	 *   BOARD_F_ARCH_CPU_INIT,
	 *   BOARD_F_MACH_CPU_INIT,
	 */
	BOARD_PHASE_FIRST,
	BOARD_F_ARCH_CPU_INIT_DM = BOARD_PHASE_FIRST,
	BOARD_F_EARLY_INIT_F,
	BOARD_F_CHECKCPU,
	BOARD_F_MISC_INIT_F,
	BOARD_F_DRAM_INIT,
	BOARD_F_RESERVE_ARCH,

	/*
	 * Post-relocation phases go here:
	 *
	 * BOARD_R_...
	 */

	BOARD_PHASE_TEST,	/* For sandbox testing */
	BOARD_PHASE_COUNT,
	BOARD_PHASE_INVALID,	/* For sandbox testing */
};

#define board_phase_mask(phase)		(1UL << (ulong)(phase))

struct board_hook {
#if CONFIG_IS_ENABLED(BOARD_HOOK_NAMES)
	const char *name;
#define _BOARD_HOOK_NAME_FIELD(_name)	.name = #_name,
#else
#define _BOARD_HOOK_NAME_FIELD(_name)
#endif
	int (*hook)(void);
	ulong phase_mask;
};

#define U_BOOT_BOARD_HOOK(_name)					\
	ll_entry_declare(struct board_hook, _name, board_hook)

#define U_BOOT_BOARD_HOOK_SINGLE(_name, _hook, _phase)			\
	U_BOOT_BOARD_HOOK(_name) = {			\
		_BOARD_HOOK_NAME_FIELD(_name)				\
		.hook = _hook,						\
		.phase_mask = board_phase_mask(_phase),			\
	}

#define U_BOOT_BOARD_HOOK_MASK(_name, _hook, _phase_mask)		\
	static const U_BOOT_BOARD_HOOK(_name) = {			\
		_BOARD_HOOK_NAME_FIELD(_name)				\
		.hook = _hook,						\
		.phase_mask = _phase_mask,				\
	}

/*
 * Return this from phase() to indicate that no more devices should handle this
 * phase
 */
#define BOARD_PHASE_CLAIMED EUSERS

/* Operations for the board driver */
struct board_ops {
	/**
	* phase() - Execute a phase of board init
	*
	 * @dev:	Device to use
	* @phase:	Phase to execute
	* @return 0 if done, -ENOSYS if not supported (which is often fine),
		BOARD_PHASE_CLAIMED if this was handled and that processing
		of this phase should stop (i.e. do not send it to other
		devices), other error if something went wrong
	*/
	int (*phase)(struct udevice *dev, enum board_phase_t phase);

	/**
	 * get_desc() - Get a description string for a board
	 *
	 * @dev:	Device to check (UCLASS_BOARD)
	 * @buf:	Buffer to place string
	 * @size:	Size of string space
	 * @return 0 if OK, -ENOSPC if buffer is too small, other -ve on error
	 */
	int (*get_desc)(struct udevice *dev, char *buf, int size);
};

#define board_get_ops(dev)        ((struct board_ops *)(dev)->driver->ops)

/* Private uclass information about each device */
struct board_uc_priv {
	ulong phase_mask;	/* Mask of phases supported by this device */
};

/**
 * board_phase() - Execute a phase of board init on a device
 *
 * @phase:	Phase to execute
 * @return 0 if done, -ENOSYS if not supported by this device, other error if
 *	something went wrong
 */
int board_phase(struct udevice *dev, enum board_phase_t phase);

/**
 * board_walk_phase() - Execute a phase of board init
 *
 * This works through the available board devices asking each one to perform
 * the requested init phase. The process continues until there are no more
 * board devices.
 *
 * If no board device provides the phase, this function returns -ENOSYS.
 *
 * @phase:	Phase to execute
 * @return 0 if done, -ENOSYS if not supported, other error if something went
 *	wrong
 */
int board_walk_phase(enum board_phase_t phase);

/**
 * board_opt_walk_phase() - Execute an optional phase of board init
 *
 * This works through the available board devices asking each one to perform
 * the requested init phase. The process continues until there are no more
 * board devices.
 *
 * If no board device provides the phase, this function returns 0.
 *
 * @phase:	Phase to execute
 * @return 0 if done, other error if something went wrong
 */
int board_walk_opt_phase(enum board_phase_t phase);

/**
 * board_walk_phase_count() - Execute an optional phase of board init
 *
 * This works through the available board devices asking each one to perform
 * the requested init phase. The process continues until there are no more
 * board devices.
 *
 * If no board provides the phase, this function returns -ENOSYS.
 *
 * @phase:	Phase to execute
 * @return number of devices which handled this phase if done, -ve error if
 *	something went wrong
 */
int board_walk_phase_count(enum board_phase_t phase, bool verbose);

/**
 * board_support_phase() - Mark a board device as supporting the given phase
 *
 * @dev:	Board device
 * @phase:	Phase to execute
 * @return 0
 */
int board_support_phase(struct udevice *dev, enum board_phase_t phase);

/**
 * board_support_phase_mask() - Mark a board device as supporting given phases
 *
 * @dev:	Board device
 * @phase_mask:	Mask of phases to execute, built up by ORing board_phase_mask()
 *		values together
 * @return 0
 */
int board_support_phase_mask(struct udevice *dev, ulong phase_mask);

int board_hook_walk_phase_count(enum board_phase_t phase);

int board_hook_walk_phase(enum board_phase_t phase);

int board_hook_walk_opt_phase(enum board_phase_t phase);

#endif
