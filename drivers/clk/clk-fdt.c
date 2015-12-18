/*
 * Copyright (C) 2015 Masahiro Yamada <yamada.masahiro@socionext.com>
 *
 * Device Tree support for clk uclass
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <clk.h>
#include <dm/uclass.h>
#include <fdtdec.h>

int fdt_clk_get(const void *fdt, int nodeoffset, int index,
		struct udevice **dev)
{
	struct fdtdec_phandle_args clkspec;
	struct udevice *clkdev;
	int rc;

	rc = fdtdec_parse_phandle_with_args(fdt, nodeoffset, "clocks",
					    "#clock-cells", 0, index, &clkspec);
	if (rc)
		return rc;

	rc = uclass_get_device_by_of_offset(UCLASS_CLK, clkspec.node, &clkdev);
	if (rc)
		return rc;

	rc = clk_get_id(clkdev, clkspec.args_count, clkspec.args);
	if (rc < 0)
		return rc;

	if (dev)
		*dev = clkdev;

	return rc;
}
