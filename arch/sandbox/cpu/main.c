// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 */

#include <asm/u-boot-sandbox.h>

int main(int argc, char *argv[])
{
	return sandbox_main(argc, argv);
}
