// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2020 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __asn_serial_h
#define __asn_serial_h

#include <dt-structs.h>

struct sandbox_serial_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_sandbox_serial dtplat;
#endif
	int colour;	/* Text colour to use for output, -1 for none */
};

struct sandbox_serial_priv {
	bool start_of_line;
};

#endif /* __asn_serial_h */
