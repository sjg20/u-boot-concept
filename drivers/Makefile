# SPDX-License-Identifier: GPL-2.0+

obj-y += soc/

# Order by directory name
obj-$(CONFIG_$(SPL_TPL_)ADC) += adc/
obj-$(CONFIG_$(SPL_TPL_)AXI) += axi/
obj-$(CONFIG_$(SPL_TPL_)BIOSEMU) += bios_emulator/
obj-$(CONFIG_$(SPL_TPL_)BOOTCOUNT_LIMIT) += bootcount/
obj-$(CONFIG_$(SPL_TPL_)BUTTON) += button/
obj-$(CONFIG_$(SPL_TPL_)CACHE) += cache/
obj-$(CONFIG_$(SPL_TPL_)CLK) += clk/
obj-$(CONFIG_$(SPL_TPL_)DM) += core/
obj-$(CONFIG_$(SPL_TPL_)CPU) += cpu/
obj-$(CONFIG_$(SPL_TPL_)CRYPTO) += crypto/
obj-$(CONFIG_$(SPL_TPL_)DM_DEMO) += demo/
obj-y += ddr/
obj-$(CONFIG_$(SPL_TPL_)DFU) += dfu/
obj-$(CONFIG_$(SPL_TPL_)DMA) += dma/
obj-$(CONFIG_$(SPL_TPL_)DMA_LEGACY) += dma/
obj-$(CONFIG_$(SPL_TPL_)FASTBOOT) += fastboot/
obj-$(CONFIG_$(SPL_)FPGA) += fpga/
obj-$(CONFIG_$(SPL_TPL_)FIRMWARE) += firmware/
obj-$(CONFIG_$(SPL_TPL_)GPIO) += gpio/
obj-$(CONFIG_$(SPL_TPL_)DM_HWSPINLOCK) += hwspinlock/
obj-$(CONFIG_$(SPL_TPL_)I2C) += i2c/
obj-$(CONFIG_$(SPL_TPL_)INPUT) += input/
obj-$(CONFIG_$(SPL_TPL_)LED) += led/
obj-$(CONFIG_$(SPL_TPL_)MAILBOX) += mailbox/
obj-y += mtd/
obj-$(CONFIG_$(SPL_TPL_)MMC) += mmc/
obj-$(CONFIG_$(SPL_TPL_)DRIVERS_MISC) += misc/
obj-$(CONFIG_$(SPL_)MULTIPLEXER) += mux/
obj-$(CONFIG_$(SPL_TPL_)NET) += net/
obj-$(CONFIG_$(SPL_TPL_)NVME) += nvme/
obj-$(CONFIG_$(SPL_TPL_)PCH) += pch/
obj-$(CONFIG_$(SPL_TPL_)PCI) += pci/
obj-$(CONFIG_$(SPL_TPL_)PCI_ENDPOINT) += pci_endpoint/
obj-$(CONFIG_$(SPL_TPL_)PHY) += phy/
obj-$(CONFIG_$(SPL_TPL_)PINCTRL) += pinctrl/
obj-$(CONFIG_$(SPL_TPL_)POWER) += power/
obj-$(CONFIG_$(SPL_TPL_)QE) += qe/
obj-$(CONFIG_$(SPL_TPL_)U_QE) += qe/
obj-$(CONFIG_$(SPL_TPL_)RAM) += ram/
obj-$(CONFIG_$(SPL_)REMOTEPROC) += remoteproc/
obj-$(CONFIG_$(SPL_TPL_)DM_RESET) += reset/
obj-$(CONFIG_$(SPL_TPL_)RTC) += rtc/
obj-$(CONFIG_$(SPL_TPL_)DM_RNG) += rng/
obj-$(CONFIG_$(SPL_TPL_)SERIAL) += serial/
obj-$(CONFIG_$(SPL_TPL_)SMEM) += smem/
obj-$(CONFIG_$(SPL_TPL_)SOUND) += sound/
obj-$(CONFIG_$(SPL_TPL_)SPI) += spi/
obj-$(CONFIG_$(SPL_TPL_)SPMI) += spmi/
obj-$(CONFIG_$(SPL_)SYSINFO) += sysinfo/
obj-$(CONFIG_$(SPL_TPL_)SYSRESET) += sysreset/
obj-$(CONFIG_$(SPL_TPL_)TEE) += tee/
obj-$(CONFIG_$(SPL_TPL_)TIMER) += timer/
obj-$(CONFIG_$(SPL_TPL_)USB) += usb/
obj-$(CONFIG_$(SPL_TPL_)VIRTIO) += virtio/
obj-$(CONFIG_$(SPL_TPL_)THERMAL) += thermal/
obj-$(CONFIG_$(SPL_TPL_)TPM) += tpm/
obj-$(CONFIG_$(SPL_TPL_)UFS) += ufs/
obj-$(CONFIG_$(SPL_TPL_)W1) += w1/
obj-$(CONFIG_$(SPL_TPL_)W1_EEPROM) += w1-eeprom/
obj-$(CONFIG_$(SPL_TPL_)WATCHDOG) += watchdog/
obj-$(CONFIG_$(SPL_TPL_)WATCHDOG_IMX_HACK) += watchdog/
obj-$(CONFIG_$(SPL_TPL_)XEN) += xen/

ifndef CONFIG_TPL_BUILD
ifdef CONFIG_SPL_BUILD

# SPL_SATA could be unified with the above if these two dirs were merged
obj-$(CONFIG_SPL_SATA) += ata/ scsi/

# CONFIG_HAVE_BLOCK_DEVICE could be unified with the above if there were a
# separate CONFIG_SPL_HAVE_BLOCK_DEVICE. This should be easier once the
# CONFIG_BLK migration is completed
obj-$(CONFIG_HAVE_BLOCK_DEVICE) += block/

endif
endif

ifeq ($(CONFIG_SPL_BUILD)$(CONFIG_TPL_BUILD),)

# These options do not have an option controlling them (e.g. ata/ needs
# CONFIG_ATA). If that were added they could be moved above.
obj-y += ata/
obj-y += block/
obj-y += bus/
obj-y += cache/
obj-y += memory/
obj-y += pwm/
obj-y += scsi/
obj-y += video/

endif
