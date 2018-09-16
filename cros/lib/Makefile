# SPDX-License-Identifier:	GPL-2.0+
#
# (C) Copyright 2018 Google, Inc
#

obj-$(CONFIG_CHROMEOS) += aux-fw-uclass.o
obj-$(CONFIG_CHROMEOS) += cros_ofnode.o
obj-$(CONFIG_CHROMEOS) += fmap.o
obj-$(CONFIG_CHROMEOS) += gbb.o
obj-$(CONFIG_CHROMEOS) += memory_wipe.o

# TODO: Drop sandbox-specific power code
ifndef CONFIG_SANDBOX
obj-$(CONFIG_CHROMEOS) += power.o
endif

obj-$(CONFIG_CHROMEOS) += secdata_tpm.o
obj-$(CONFIG_CHROMEOS) += vboot.o
obj-$(CONFIG_CHROMEOS) += vboot-ec-uclass.o
