# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2021 Google LLC
# Written by Simon Glass <sjg@chromium.org>

obj-$(CONFIG_SYS_FSL_DDR) += fsl/
obj-$(CONFIG_SYS_FSL_MMDC) += fsl/
obj-$(CONFIG_$(SPL_TPL_)ALTERA_SDRAM) += altera/

obj-$(CONFIG_$(SPL_TPL_)MPC8XXX_INIT_DDR) += fsl/
obj-$(CONFIG_ARMADA_38X) += marvell/a38x/
obj-$(CONFIG_ARMADA_XP) += marvell/axp/
obj-$(CONFIG_$(SPL_TPL_)ALTERA_SDRAM) += altera/

# This ifdef is necessary because these is no Kconfig option for DDR
ifdef CONFIG_SPL_BUILD
obj-$(CONFIG_ARCH_IMX8M) += imx/imx8m/
endif

obj-$(CONFIG_$(SPL_TPL_)MPC8XXX_INIT_DDR) += fsl/
obj-$(CONFIG_MACH_PIC32) += microchip/
