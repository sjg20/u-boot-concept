/*
 * From coreboot southbridge/intel/bd82x6x/lpc.c
 *
 * Copyright (C) 2008-2009 coresystems GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <errno.h>
#include <fdtdec.h>
#include <rtc.h>
#include <pci.h>
#include <asm/acpi.h>
#include <asm/i8259.h>
#include <asm/ioapic.h>
#include <asm/isa_dma.h>
#include <asm/arch/pch.h>

#define NMI_OFF	0

#define ENABLE_ACPI_MODE_IN_COREBOOT	0
#define TEST_SMM_FLASH_LOCKDOWN		0

typedef struct southbridge_intel_bd82x6x_config config_t;

static void pch_enable_apic(pci_dev_t dev)
{
	int i;
	u32 reg32;
	volatile u32 *ioapic_index = (volatile u32 *)(IO_APIC_ADDR);
	volatile u32 *ioapic_data = (volatile u32 *)(IO_APIC_ADDR + 0x10);

	/* Enable ACPI I/O and power management.
	 * Set SCI IRQ to IRQ9
	 */
	pci_write_config8(dev, ACPI_CNTL, 0x80);

	*ioapic_index = 0;
	*ioapic_data = (1 << 25);

	/* affirm full set of redirection table entries ("write once") */
	*ioapic_index = 1;
	reg32 = *ioapic_data;
	*ioapic_index = 1;
	*ioapic_data = reg32;

	*ioapic_index = 0;
	reg32 = *ioapic_data;
	debug("Southbridge APIC ID = %x\n", (reg32 >> 24) & 0x0f);
	if (reg32 != (1 << 25))
		panic("APIC Error\n");

	debug("Dumping IOAPIC registers\n");
	for (i=0; i<3; i++) {
		*ioapic_index = i;
		debug("  reg 0x%04x:", i);
		reg32 = *ioapic_data;
		debug(" 0x%08x\n", reg32);
	}

	*ioapic_index = 3; /* Select Boot Configuration register. */
	*ioapic_data = 1; /* Use Processor System Bus to deliver interrupts. */
}

static void pch_enable_serial_irqs(pci_dev_t dev)
{
	/* Set packet length and toggle silent mode bit for one frame. */
	pci_write_config8(dev, SERIRQ_CNTL,
			  (1 << 7) | (1 << 6) | ((21 - 17) << 2) | (0 << 0));
#if !CONFIG_SERIRQ_CONTINUOUS_MODE
	pci_write_config8(dev, SERIRQ_CNTL,
			  (1 << 7) | (0 << 6) | ((21 - 17) << 2) | (0 << 0));
#endif
}

static int pch_pirq_init(const void *blob, int node, pci_dev_t dev)
{
	uint8_t route[8], *ptr;

	if (fdtdec_get_byte_array(blob, node, "pirq-routing", route,
				  sizeof(route)))
		return -EINVAL;
	ptr = route;
	pci_write_config8(dev, PIRQA_ROUT, *ptr++);
	pci_write_config8(dev, PIRQB_ROUT, *ptr++);
	pci_write_config8(dev, PIRQC_ROUT, *ptr++);
	pci_write_config8(dev, PIRQD_ROUT, *ptr++);

	pci_write_config8(dev, PIRQE_ROUT, *ptr++);
	pci_write_config8(dev, PIRQF_ROUT, *ptr++);
	pci_write_config8(dev, PIRQG_ROUT, *ptr++);
	pci_write_config8(dev, PIRQH_ROUT, *ptr++);

	/*
	 * TODO(sjg@chromium.org): U-Boot does not set up the interrupts
	 * here. It's unclear if it is needed
	 */
	return 0;
}

static int pch_gpi_routing(const void *blob, int node, pci_dev_t dev)
{
	u8 route[16];
	u32 reg;
	int gpi;

	if (fdtdec_get_byte_array(blob, node, "gpi-routing", route,
				  sizeof(route)))
		return -EINVAL;

	for (reg = 0, gpi = 0; gpi < ARRAY_SIZE(route); gpi++)
		reg |= route[gpi] << (gpi * 2);

	pci_write_config32(dev, 0xb8, reg);

	return 0;
}

static int pch_power_options(const void *blob, int node, pci_dev_t dev)
{
	u8 reg8;
	u16 reg16, pmbase;
	u32 reg32;
	const char *state;
	int pwr_on;
	int nmi_option;
	int ret;

	/*
	 * Which state do we want to goto after g3 (power restored)?
	 * 0 == S0 Full On
	 * 1 == S5 Soft Off
	 *
	 * If the option is not existent (Laptops), use Kconfig setting.
	 * TODO(sjg@chromium.org): Make this configurable
	 */
	pwr_on = MAINBOARD_POWER_ON;

	reg16 = pci_read_config16(dev, GEN_PMCON_3);
	reg16 &= 0xfffe;
	switch (pwr_on) {
	case MAINBOARD_POWER_OFF:
		reg16 |= 1;
		state = "off";
		break;
	case MAINBOARD_POWER_ON:
		reg16 &= ~1;
		state = "on";
		break;
	case MAINBOARD_POWER_KEEP:
		reg16 &= ~1;
		state = "state keep";
		break;
	default:
		state = "undefined";
	}

	reg16 &= ~(3 << 4);	/* SLP_S4# Assertion Stretch 4s */
	reg16 |= (1 << 3);	/* SLP_S4# Assertion Stretch Enable */

	reg16 &= ~(1 << 10);
	reg16 |= (1 << 11);	/* SLP_S3# Min Assertion Width 50ms */

	reg16 |= (1 << 12);	/* Disable SLP stretch after SUS well */

	pci_write_config16(dev, GEN_PMCON_3, reg16);
	debug("Set power %s after power failure.\n", state);

	/* Set up NMI on errors. */
	reg8 = inb(0x61);
	reg8 &= 0x0f;		/* Higher Nibble must be 0 */
	reg8 &= ~(1 << 3);	/* IOCHK# NMI Enable */
	reg8 |= (1 << 2); /* PCI SERR# Disable for now */
	outb(reg8, 0x61);

	reg8 = inb(0x70);
	/* TODO(sjg@chromium.org): Make this configurable */
	nmi_option = NMI_OFF;
	if (nmi_option) {
		debug("NMI sources enabled.\n");
		reg8 &= ~(1 << 7);	/* Set NMI. */
	} else {
		debug("NMI sources disabled.\n");
		reg8 |= ( 1 << 7);	/* Can't mask NMI from PCI-E and NMI_NOW */
	}
	outb(reg8, 0x70);

	/* Enable CPU_SLP# and Intel Speedstep, set SMI# rate down */
	reg16 = pci_read_config16(dev, GEN_PMCON_1);
	reg16 &= ~(3 << 0);	// SMI# rate 1 minute
	reg16 &= ~(1 << 10);	// Disable BIOS_PCI_EXP_EN for native PME
#if DEBUG_PERIODIC_SMIS
	/* Set DEBUG_PERIODIC_SMIS in pch.h to debug using
	 * periodic SMIs.
	 */
	reg16 |= (3 << 0); // Periodic SMI every 8s
#endif
	pci_write_config16(dev, GEN_PMCON_1, reg16);

	// Set the board's GPI routing.
	ret = pch_gpi_routing(blob, node, dev);
	if (ret)
		return ret;

	pmbase = pci_read_config16(dev, 0x40) & 0xfffe;

	writel(pmbase + GPE0_EN, fdtdec_get_int(blob, node, "gpe0-enable", 0));
	writew(pmbase + ALT_GP_SMI_EN, fdtdec_get_int(blob, node,
						      "alt-gp-smi-enable", 0));

	/* Set up power management block and determine sleep mode */
	reg32 = inl(pmbase + 0x04); // PM1_CNT
	reg32 &= ~(7 << 10);	// SLP_TYP
	reg32 |= (1 << 0);	// SCI_EN
	outl(reg32, pmbase + 0x04);

	/* Clear magic status bits to prevent unexpected wake */
	reg32 = RCBA32(0x3310);
	reg32 |= (1 << 4)|(1 << 5)|(1 << 0);
	RCBA32(0x3310) = reg32;

	reg32 = RCBA32(0x3f02);
	reg32 &= ~0xf;
	RCBA32(0x3f02) = reg32;

	return 0;
}

static void pch_rtc_init(pci_dev_t dev)
{
	u8 reg8;
	int rtc_failed;

	reg8 = pci_read_config8(dev, GEN_PMCON_3);
	rtc_failed = reg8 & RTC_BATTERY_DEAD;
	if (rtc_failed) {
		reg8 &= ~RTC_BATTERY_DEAD;
		pci_write_config8(dev, GEN_PMCON_3, reg8);
	}
	debug("rtc_failed = 0x%x\n", rtc_failed);

#if CONFIG_HAVE_ACPI_RESUME
	/* Avoid clearing pending interrupts and resetting the RTC control
	 * register in the resume path because the Linux kernel relies on
	 * this to know if it should restart the RTC timerqueue if the wake
	 * was due to the RTC alarm.
	 */
	if (acpi_get_slp_type() == 3)
		return;
#endif
	rtc_init(rtc_failed);
}

/* CougarPoint PCH Power Management init */
static void cpt_pm_init(pci_dev_t dev)
{
	debug("CougarPoint PM init\n");
	pci_write_config8(dev, 0xa9, 0x47);
	RCBA32_AND_OR(0x2238, ~0UL, (1 << 6)|(1 << 0));
	RCBA32_AND_OR(0x228c, ~0UL, (1 << 0));
	RCBA16_AND_OR(0x1100, ~0UL, (1 << 13)|(1 << 14));
	RCBA16_AND_OR(0x0900, ~0UL, (1 << 14));
	RCBA32(0x2304) = 0xc0388400;
	RCBA32_AND_OR(0x2314, ~0UL, (1 << 5)|(1 << 18));
	RCBA32_AND_OR(0x2320, ~0UL, (1 << 15)|(1 << 1));
	RCBA32_AND_OR(0x3314, ~0x1f, 0xf);
	RCBA32(0x3318) = 0x050f0000;
	RCBA32(0x3324) = 0x04000000;
	RCBA32_AND_OR(0x3340, ~0UL, 0xfffff);
	RCBA32_AND_OR(0x3344, ~0UL, (1 << 1));
	RCBA32(0x3360) = 0x0001c000;
	RCBA32(0x3368) = 0x00061100;
	RCBA32(0x3378) = 0x7f8fdfff;
	RCBA32(0x337c) = 0x000003fc;
	RCBA32(0x3388) = 0x00001000;
	RCBA32(0x3390) = 0x0001c000;
	RCBA32(0x33a0) = 0x00000800;
	RCBA32(0x33b0) = 0x00001000;
	RCBA32(0x33c0) = 0x00093900;
	RCBA32(0x33cc) = 0x24653002;
	RCBA32(0x33d0) = 0x062108fe;
	RCBA32_AND_OR(0x33d4, 0xf000f000, 0x00670060);
	RCBA32(0x3a28) = 0x01010000;
	RCBA32(0x3a2c) = 0x01010404;
	RCBA32(0x3a80) = 0x01041041;
	RCBA32_AND_OR(0x3a84, ~0x0000ffff, 0x00001001);
	RCBA32_AND_OR(0x3a84, ~0UL, (1 << 24)); /* SATA 2/3 disabled */
	RCBA32_AND_OR(0x3a88, ~0UL, (1 << 0));  /* SATA 4/5 disabled */
	RCBA32(0x3a6c) = 0x00000001;
	RCBA32_AND_OR(0x2344, 0x00ffff00, 0xff00000c);
	RCBA32_AND_OR(0x80c, ~(0xff << 20), 0x11 << 20);
	RCBA32(0x33c8) = 0;
	RCBA32_AND_OR(0x21b0, ~0UL, 0xf);
}

/* PantherPoint PCH Power Management init */
static void ppt_pm_init(pci_dev_t dev)
{
	debug("PantherPoint PM init\n");
	pci_write_config8(dev, 0xa9, 0x47);
	RCBA32_AND_OR(0x2238, ~0UL, (1 << 0));
	RCBA32_AND_OR(0x228c, ~0UL, (1 << 0));
	RCBA16_AND_OR(0x1100, ~0UL, (1 << 13)|(1 << 14));
	RCBA16_AND_OR(0x0900, ~0UL, (1 << 14));
	RCBA32(0x2304) = 0xc03b8400;
	RCBA32_AND_OR(0x2314, ~0UL, (1 << 5)|(1 << 18));
	RCBA32_AND_OR(0x2320, ~0UL, (1 << 15)|(1 << 1));
	RCBA32_AND_OR(0x3314, ~0x1f, 0xf);
	RCBA32(0x3318) = 0x054f0000;
	RCBA32(0x3324) = 0x04000000;
	RCBA32_AND_OR(0x3340, ~0UL, 0xfffff);
	RCBA32_AND_OR(0x3344, ~0UL, (1 << 1)|(1 << 0));
	RCBA32(0x3360) = 0x0001c000;
	RCBA32(0x3368) = 0x00061100;
	RCBA32(0x3378) = 0x7f8fdfff;
	RCBA32(0x337c) = 0x000003fd;
	RCBA32(0x3388) = 0x00001000;
	RCBA32(0x3390) = 0x0001c000;
	RCBA32(0x33a0) = 0x00000800;
	RCBA32(0x33b0) = 0x00001000;
	RCBA32(0x33c0) = 0x00093900;
	RCBA32(0x33cc) = 0x24653002;
	RCBA32(0x33d0) = 0x067388fe;
	RCBA32_AND_OR(0x33d4, 0xf000f000, 0x00670060);
	RCBA32(0x3a28) = 0x01010000;
	RCBA32(0x3a2c) = 0x01010404;
	RCBA32(0x3a80) = 0x01040000;
	RCBA32_AND_OR(0x3a84, ~0x0000ffff, 0x00001001);
	RCBA32_AND_OR(0x3a84, ~0UL, (1 << 24)); /* SATA 2/3 disabled */
	RCBA32_AND_OR(0x3a88, ~0UL, (1 << 0));  /* SATA 4/5 disabled */
	RCBA32(0x3a6c) = 0x00000001;
	RCBA32_AND_OR(0x2344, 0x00ffff00, 0xff00000c);
	RCBA32_AND_OR(0x80c, ~(0xff << 20), 0x11 << 20);
	RCBA32_AND_OR(0x33a4, ~0UL, (1 << 0));
	RCBA32(0x33c8) = 0;
	RCBA32_AND_OR(0x21b0, ~0UL, 0xf);
}

static void enable_hpet(void)
{
	u32 reg32;

	/* Move HPET to default address 0xfed00000 and enable it */
	reg32 = RCBA32(HPTC);
	reg32 |= (1 << 7); // HPET Address Enable
	reg32 &= ~(3 << 0);
	RCBA32(HPTC) = reg32;
}

static void enable_clock_gating(pci_dev_t dev)
{
	u32 reg32;
	u16 reg16;

	RCBA32_AND_OR(0x2234, ~0UL, 0xf);

	reg16 = pci_read_config16(dev, GEN_PMCON_1);
	reg16 |= (1 << 2) | (1 << 11);
	pci_write_config16(dev, GEN_PMCON_1, reg16);

	pch_iobp_update(0xEB007F07, ~0UL, (1 << 31));
	pch_iobp_update(0xEB004000, ~0UL, (1 << 7));
	pch_iobp_update(0xEC007F07, ~0UL, (1 << 31));
	pch_iobp_update(0xEC004000, ~0UL, (1 << 7));

	reg32 = RCBA32(CG);
	reg32 |= (1 << 31);
	reg32 |= (1 << 29) | (1 << 28);
	reg32 |= (1 << 27) | (1 << 26) | (1 << 25) | (1 << 24);
	reg32 |= (1 << 16);
	reg32 |= (1 << 17);
	reg32 |= (1 << 18);
	reg32 |= (1 << 22);
	reg32 |= (1 << 23);
	reg32 &= ~(1 << 20);
	reg32 |= (1 << 19);
	reg32 |= (1 << 0);
	reg32 |= (0xf << 1);
	RCBA32(CG) = reg32;

	RCBA32_OR(0x38c0, 0x7);
	RCBA32_OR(0x36d4, 0x6680c004);
	RCBA32_OR(0x3564, 0x3);
}

#if CONFIG_HAVE_SMI_HANDLER
static void pch_lock_smm(pci_dev_t dev)
{
#if TEST_SMM_FLASH_LOCKDOWN
	u8 reg8;
#endif

	if (acpi_slp_type != 3) {
#if ENABLE_ACPI_MODE_IN_COREBOOT
		debug("Enabling ACPI via APMC:\n");
		outb(0xe1, 0xb2); // Enable ACPI mode
		debug("done.\n");
#else
		debug("Disabling ACPI via APMC:\n");
		outb(0x1e, 0xb2); // Disable ACPI mode
		debug("done.\n");
#endif
	}

	/* Don't allow evil boot loaders, kernels, or
	 * userspace applications to deceive us:
	 */
	smm_lock();

#if TEST_SMM_FLASH_LOCKDOWN
	/* Now try this: */
	debug("Locking BIOS to RO... ");
	reg8 = pci_read_config8(dev, 0xdc);	/* BIOS_CNTL */
	debug(" BLE: %s; BWE: %s\n", (reg8&2)?"on":"off",
			(reg8&1)?"rw":"ro");
	reg8 &= ~(1 << 0);			/* clear BIOSWE */
	pci_write_config8(dev, 0xdc, reg8);
	reg8 |= (1 << 1);			/* set BLE */
	pci_write_config8(dev, 0xdc, reg8);
	debug("ok.\n");
	reg8 = pci_read_config8(dev, 0xdc);	/* BIOS_CNTL */
	debug(" BLE: %s; BWE: %s\n", (reg8&2)?"on":"off",
			(reg8&1)?"rw":"ro");

	debug("Writing:\n");
	*(volatile u8 *)0xfff00000 = 0x00;
	debug("Testing:\n");
	reg8 |= (1 << 0);			/* set BIOSWE */
	pci_write_config8(dev, 0xdc, reg8);

	reg8 = pci_read_config8(dev, 0xdc);	/* BIOS_CNTL */
	debug(" BLE: %s; BWE: %s\n", (reg8&2)?"on":"off",
			(reg8&1)?"rw":"ro");
	debug("Done.\n");
#endif
}
#endif

static void pch_disable_smm_only_flashing(pci_dev_t dev)
{
	u8 reg8;

	debug("Enabling BIOS updates outside of SMM... ");
	reg8 = pci_read_config8(dev, 0xdc);	/* BIOS_CNTL */
	reg8 &= ~(1 << 5);
	pci_write_config8(dev, 0xdc, reg8);
}

static void pch_fixups(pci_dev_t dev)
{
	u8 gen_pmcon_2;

	/* Indicate DRAM init done for MRC S3 to know it can resume */
	gen_pmcon_2 = pci_read_config8(dev, GEN_PMCON_2);
	gen_pmcon_2 |= (1 << 7);
	pci_write_config8(dev, GEN_PMCON_2, gen_pmcon_2);

	/*
	 * Enable DMI ASPM in the PCH
	 */
	RCBA32_AND_OR(0x2304, ~(1 << 10), 0);
	RCBA32_OR(0x21a4, (1 << 11)|(1 << 10));
	RCBA32_OR(0x21a8, 0x3);
}

int lpc_early_init(const void *blob, int node, pci_dev_t dev)
{
	struct reg_info {
		u32 base;
		u32 size;
	} values[4], *ptr;
	int count;
	int i;

	count = fdtdec_get_int_array_count(blob, node, "gen-dec",
			(u32 *)values, sizeof(values) / sizeof(u32));
	if (count < 0)
		return -EINVAL;

	/* Set COM1/COM2 decode range */
	pci_write_config16(dev, LPC_IO_DEC, 0x0010);

	/* Enable PS/2 Keyboard/Mouse, EC areas and COM1 */
	pci_write_config16(dev, LPC_EN, KBC_LPC_EN | MC_LPC_EN |
			      GAMEL_LPC_EN | COMA_LPC_EN);

	/* Write all registers but use 0 if we run out of data */
	count = count * sizeof(u32) / sizeof(values[0]);
	for (i = 0, ptr = values; i < ARRAY_SIZE(values); i++, ptr++) {
		u32 reg = 0;

		if (i < count)
			reg = ptr->base | PCI_COMMAND_IO | (ptr->size << 16);
		pci_write_config32(dev, LPC_GENx_DEC(i), reg);
	}

	return 0;
}

int lpc_init(struct pci_controller *hose, pci_dev_t dev)
{
	const void *blob = gd->fdt_blob;
	int node;

	debug("pch: lpc_init\n");
	pci_config_fixed(hose, dev, 0, 0);
	pci_config_fixed(hose, dev, 1, 0xff800000);
	pci_config_fixed(hose, dev, 2, 0xfec00000);
	pci_config_fixed(hose, dev, 3, 0x800);
	pci_config_fixed(hose, dev, 4, 0x900);

	node = fdtdec_next_compatible(blob, 0, COMPAT_INTEL_LPC);
	if (node < 0)
		return -ENOENT;

	/* Set the value for PCI command register. */
	pci_write_config16(dev, PCI_COMMAND, 0x000f);

	/* IO APIC initialization. */
	pch_enable_apic(dev);

	pch_enable_serial_irqs(dev);

	/* Setup the PIRQ. */
	pch_pirq_init(blob, node, dev);

	/* Setup power options. */
	pch_power_options(blob, node, dev);

	/* Initialize power management */
	switch (pch_silicon_type()) {
	case PCH_TYPE_CPT: /* CougarPoint */
		cpt_pm_init(dev);
		break;
	case PCH_TYPE_PPT: /* PantherPoint */
		ppt_pm_init(dev);
		break;
	default:
		printf("Unknown Chipset: %#02x.%dx\n", PCI_DEV(dev),
		       PCI_FUNC(dev));
		return -ENOSYS;
	}

	/* Set the state of the GPIO lines. */
	//gpio_init(dev);

	/* Initialize the real time clock. */
	pch_rtc_init(dev);

	/* Initialize ISA DMA. */
	isa_dma_init();

	/* Initialize the High Precision Event Timers, if present. */
	enable_hpet();

	/* Initialize Clock Gating */
	enable_clock_gating(dev);

	i8259_setup();

	/* Interrupt 9 should be level triggered (SCI). The OS might do this */
	i8259_configure_irq_trigger(9, true);

	pch_disable_smm_only_flashing(dev);

#if CONFIG_HAVE_SMI_HANDLER
	pch_lock_smm(dev);
#endif

	pch_fixups(dev);

	return 0;
}

void lpc_enable(pci_dev_t dev)
{
	/* Enable PCH Display Port */
	RCBA16(DISPBDF) = 0x0010;
	RCBA32_OR(FD2, PCH_ENABLE_DBDF);
// 	pch_enable(dev);
}
