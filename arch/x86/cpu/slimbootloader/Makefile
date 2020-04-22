# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2019-2020 Intel Corporation <www.intel.com>

ifeq ($(CONFIG_X86_64),y)
obj-y += entry64.o
else
obj-y += car.o
endif
obj-y += slimbootloader.o sdram.o serial.o
