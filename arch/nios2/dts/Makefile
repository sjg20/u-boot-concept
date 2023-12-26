# SPDX-License-Identifier: GPL-2.0+

dtb-$(CONFIG_NIOS2) += 10m50_devboard.dtb 3c120_devboard.dtb

include $(srctree)/scripts/Makefile.dts

targets += $(dtb-y)

DTC_FLAGS += -R 4 -p 0x1000

PHONY += dtbs
dtbs: $(addprefix $(obj)/, $(dtb-y))
	@:

clean-files := *.dtb
