# SPDX-License-Identifier: GPL-2.0+
#

obj-$(CONFIG_$(SPL_)ACPIGEN) += acpigen.o
obj-$(CONFIG_$(SPL_)ACPIGEN) += acpi_device.o
obj-$(CONFIG_$(SPL_)ACPIGEN) += acpi_dp.o
obj-$(CONFIG_$(SPL_)ACPIGEN) += acpi_table.o
ifndef CONFIG_QFW
obj-y += acpi_writer.o
endif

obj-y += base.o
