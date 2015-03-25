#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Alternatively, this software may be distributed under the terms of the
# GNU General Public License ("GPL") version 2 as published by the Free
# Software Foundation.
#

include $(TOPDIR)/config.mk

LIB	= $(obj)libcros.o

COBJS-$(CONFIG_CHROMEOS) += boot_kernel.o
COBJS-$(CONFIG_CHROMEOS) += cros_init.o
COBJS-$(CONFIG_CHROMEOS) += crossystem_data.o
ifeq ($(CONFIG_CHROMEOS),y)
COBJS-$(CONFIG_OF_CONTROL) += cros_fdtdec.o
endif
COBJS-$(CONFIG_CHROMEOS_SPI) += firmware_storage_spi.o
COBJS-$(CONFIG_CHROMEOS_MMC) += firmware_storage_mmc.o
COBJS-$(CONFIG_CHROMEOS) += fmap.o
COBJS-$(CONFIG_CHROMEOS) += gbb.o
COBJS-$(CONFIG_CHROMEOS) += memory_wipe.o
COBJS-$(CONFIG_CHROMEOS) += vboot_flag.o
COBJS-$(CONFIG_CHROMEOS_CONST_FLAG) += vboot_flag_const.o
COBJS-$(CONFIG_CHROMEOS_GPIO_FLAG) += vboot_flag_gpio.o
COBJS-$(CONFIG_CHROMEOS_KEY_FLAG) += vboot_flag_key.o
COBJS-$(CONFIG_CHROMEOS_CROS_EC_FLAG) += vboot_flag_cros_ec.o
COBJS-$(CONFIG_CHROMEOS) += vboot_flag_debug.o
COBJS-$(CONFIG_CHROMEOS) += vboot.o vboot_dump.o vboot_fdt.o
COBJS-$(CONFIG_CHROMEOS) += readonly.o
COBJS-$(CONFIG_CHROMEOS) += readwrite.o

COBJS	:= $(COBJS-y)
OBJS	:= $(addprefix $(obj),$(COBJS))

$(LIB):	$(obj).depend $(OBJS)
	$(call cmd_link_o_target, $(OBJS) $(SOBJS))

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
