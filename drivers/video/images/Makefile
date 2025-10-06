# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Simon Glass <sjg@chromium.org>

obj-$(CONFIG_VIDEO_LOGO) += u_boot.o
ifdef CONFIG_MOUSE
obj-$(CONFIG_EXPO) += riscos_arrow.o
endif

ifdef CONFIG_$(PHASE_)GENERATE_ACPI_TABLE
obj-y += bgrt.o
endif
