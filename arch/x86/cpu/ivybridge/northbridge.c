/*
 * From Coreboot northbridge/intel/sandybridge/northbridge.c
 *
 * Copyright (C) 2007-2009 coresystems GmbH
 * Copyright (C) 2011 The Chromium Authors
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/msr.h>
#include <asm/acpi.h>
#include <asm/processor.h>
#include <asm/arch/pch.h>
#include <asm/arch/model_206ax.h>
#include <asm/arch/sandybridge.h>

static int bridge_revision_id = -1;

int bridge_silicon_revision(void)
{
	if (bridge_revision_id < 0) {
		struct cpuid_result result;
		uint8_t stepping, bridge_id;
		pci_dev_t dev;

		result = cpuid(1);
		stepping = result.eax & 0xf;
		dev = PCI_BDF_CB(0, 0, 0);
		bridge_id = pci_read_config16(dev, PCI_DEVICE_ID) & 0xf0;
		bridge_revision_id = bridge_id | stepping;
	}

	return bridge_revision_id;
}

/*
 * Reserve everything between A segment and 1MB:
 *
 * 0xa0000 - 0xbffff: legacy VGA
 * 0xc0000 - 0xcffff: VGA OPROM (needed by kernel)
 * 0xe0000 - 0xfffff: SeaBIOS, if used, otherwise DMI
 */
static const int legacy_hole_base_k = 0xa0000 / 1024;
static const int legacy_hole_size_k = 384;

static int get_pcie_bar(u32 *base, u32 *len)
{
	pci_dev_t dev = PCI_BDF_CB(0, 0, 0);
	u32 pciexbar_reg;

	*base = 0;
	*len = 0;

	pciexbar_reg = pci_read_config32(dev, PCIEXBAR);

	if (!(pciexbar_reg & (1 << 0)))
		return 0;

	switch ((pciexbar_reg >> 1) & 3) {
	case 0: // 256MB
		*base = pciexbar_reg & ((1 << 31)|(1 << 30)|(1 << 29)|(1 << 28));
		*len = 256 * 1024 * 1024;
		return 1;
	case 1: // 128M
		*base = pciexbar_reg & ((1 << 31)|(1 << 30)|(1 << 29)|(1 << 28)|(1 << 27));
		*len = 128 * 1024 * 1024;
		return 1;
	case 2: // 64M
		*base = pciexbar_reg & ((1 << 31)|(1 << 30)|(1 << 29)|(1 << 28)|(1 << 27)|(1 << 26));
		*len = 64 * 1024 * 1024;
		return 1;
	}

	return 0;
}

static void add_fixed_resources(pci_dev_t dev, int index)
{
	u32 pcie_config_base, pcie_config_size;

	/* Using uma_resource() here would fail as base & size cannot
	 * be used as-is for a single MTRR. This would cause excessive
	 * use of MTRRs.
	 *
	 * Use of mmio_resource() instead does not create UC holes by using
	 * MTRRs, but making these regions uncacheable is taken care of by
	 * making sure they do not overlap with any ram_resource().
	 *
	 * The resources can be changed to use separate mmio_resource()
	 * calls after MTRR code is able to merge them wisely.
	 */
// 	mmio_resource(dev, index++, uma_memory_base >> 10, uma_memory_size >> 10);

	if (get_pcie_bar(&pcie_config_base, &pcie_config_size)) {
		debug("Adding PCIe config bar base=0x%08x "
		       "size=0x%x\n", pcie_config_base, pcie_config_size);
	}
#if 0
`		resource = new_resource(dev, index++);
		resource->base = (resource_t) pcie_config_base;
		resource->size = (resource_t) pcie_config_size;
		resource->flags = IORESOURCE_MEM | IORESOURCE_RESERVE |
		    IORESOURCE_FIXED | IORESOURCE_STORED | IORESOURCE_ASSIGNED;
	}
	mmio_resource(dev, index++, legacy_hole_base_k,
			(0xc0000 >> 10) - legacy_hole_base_k);
	reserved_ram_resource(dev, index++, 0xc0000 >> 10,
			(0x100000 - 0xc0000) >> 10);

#if CONFIG_CHROMEOS_RAMOOPS
	reserved_ram_resource(dev, index++,
			CONFIG_CHROMEOS_RAMOOPS_RAM_START >> 10,
			CONFIG_CHROMEOS_RAMOOPS_RAM_SIZE >> 10);
#endif

	/* Required for SandyBridge sighting 3715511 */
	bad_ram_resource(dev, index++, 0x20000000 >> 10, 0x00200000 >> 10);
	bad_ram_resource(dev, index++, 0x40000000 >> 10, 0x00200000 >> 10);
#endif
}

#if 0
	/* TODO We could determine how many PCIe busses we need in
	 * the bar. For now that number is hardcoded to a max of 64.
	 * See e7525/northbridge.c for an example.
	 */
static struct device_operations pci_domain_ops = {
	.read_resources   = pci_domain_read_resources,
	.set_resources    = pci_domain_set_resources,
	.enable_resources = NULL,
	.init             = NULL,
	.scan_bus         = pci_domain_scan_bus,
#if CONFIG_MMCONF_SUPPORT_DEFAULT
	.ops_pci_bus	  = &pci_ops_mmconf,
#else
	.ops_pci_bus	  = &pci_cf8_conf1,
#endif
};

static void mc_read_resources(pci_dev_t dev)
{
	struct resource *resource;

	pci_dev_read_resources(dev);

	/* So, this is one of the big mysteries in the coreboot resource
	 * allocator. This resource should make sure that the address space
	 * of the PCIe memory mapped config space bar. But it does not.
	 */

	/* We use 0xcf as an unused index for our PCIe bar so that we find it again */
	resource = new_resource(dev, 0xcf);
	resource->base = DEFAULT_PCIEXBAR;
	resource->size = 64 * 1024 * 1024;	/* 64MB hard coded PCIe config space */
	resource->flags =
	    IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_STORED |
	    IORESOURCE_ASSIGNED;
	debug("Adding PCIe enhanced config space BAR 0x%08lx-0x%08lx.\n",
		     (unsigned long)(resource->base), (unsigned long)(resource->base + resource->size));
}

static void mc_set_resources(device_t dev)
{
	struct resource *resource;

	/* Report the PCIe BAR */
	resource = find_resource(dev, 0xcf);
	if (resource) {
		report_resource_stored(dev, resource, "<mmconfig>");
	}

	/* And call the normal set_resources */
	pci_dev_set_resources(dev);
}

static void intel_set_subsystem(device_t dev, unsigned vendor, unsigned device)
{
	if (!vendor || !device) {
		pci_write_config32(dev, PCI_SUBSYSTEM_VENDOR_ID,
				pci_read_config32(dev, PCI_VENDOR_ID));
	} else {
		pci_write_config32(dev, PCI_SUBSYSTEM_VENDOR_ID,
				((device & 0xffff) << 16) | (vendor & 0xffff));
	}
}
#endif

static void northbridge_dmi_init(pci_dev_t dev)
{
	u32 reg32;

	/* Clear error status bits */
	DMIBAR32(0x1c4) = 0xffffffff;
	DMIBAR32(0x1d0) = 0xffffffff;

	/* Steps prior to DMI ASPM */
	if ((bridge_silicon_revision() & BASE_REV_MASK) == BASE_REV_SNB) {
		reg32 = DMIBAR32(0x250);
		reg32 &= ~((1 << 22)|(1 << 20));
		reg32 |= (1 << 21);
		DMIBAR32(0x250) = reg32;
	}

	reg32 = DMIBAR32(0x238);
	reg32 |= (1 << 29);
	DMIBAR32(0x238) = reg32;

	if (bridge_silicon_revision() >= SNB_STEP_D0) {
		reg32 = DMIBAR32(0x1f8);
		reg32 |= (1 << 16);
		DMIBAR32(0x1f8) = reg32;
	} else if (bridge_silicon_revision() >= SNB_STEP_D1) {
		reg32 = DMIBAR32(0x1f8);
		reg32 &= ~(1 << 26);
		reg32 |= (1 << 16);
		DMIBAR32(0x1f8) = reg32;

		reg32 = DMIBAR32(0x1fc);
		reg32 |= (1 << 12) | (1 << 23);
		DMIBAR32(0x1fc) = reg32;
	}

	/* Enable ASPM on SNB link, should happen before PCH link */
	if ((bridge_silicon_revision() & BASE_REV_MASK) == BASE_REV_SNB) {
		reg32 = DMIBAR32(0xd04);
		reg32 |= (1 << 4);
		DMIBAR32(0xd04) = reg32;
	}

	reg32 = DMIBAR32(0x88);
	reg32 |= (1 << 1) | (1 << 0);
	DMIBAR32(0x88) = reg32;
}

void northbridge_init(pci_dev_t dev)
{
	u8 bios_reset_cpl;
	u32 bridge_type;

	add_fixed_resources(dev, 6);
	northbridge_dmi_init(dev);

	bridge_type = MCHBAR32(0x5f10);
	bridge_type &= ~0xff;

	if ((bridge_silicon_revision() & BASE_REV_MASK) == BASE_REV_IVB) {
		/* Enable Power Aware Interrupt Routing */
		u8 pair = MCHBAR8(0x5418);
		pair &= ~0xf;	/* Clear 3:0 */
		pair |= 0x4;	/* Fixed Priority */
		MCHBAR8(0x5418) = pair;

		/* 30h for IvyBridge */
		bridge_type |= 0x30;
	} else {
		/* 20h for Sandybridge */
		bridge_type |= 0x20;
	}
	MCHBAR32(0x5f10) = bridge_type;

	/*
	 * Set bit 0 of BIOS_RESET_CPL to indicate to the CPU
	 * that BIOS has initialized memory and power management
	 */
	bios_reset_cpl = MCHBAR8(BIOS_RESET_CPL);
	bios_reset_cpl |= 1;
	MCHBAR8(BIOS_RESET_CPL) = bios_reset_cpl;
	debug("Set BIOS_RESET_CPL\n");

	/* Configure turbo power limits 1ms after reset complete bit */
	mdelay(1);
	set_power_limits(28);

	/*
	 * CPUs with configurable TDP also need power limits set
	 * in MCHBAR.  Use same values from MSR_PKG_POWER_LIMIT.
	 */
	if (cpu_config_tdp_levels()) {
		msr_t msr = msr_read(MSR_PKG_POWER_LIMIT);
		MCHBAR32(0x59A0) = msr.lo;
		MCHBAR32(0x59A4) = msr.hi;
	}

	/* Set here before graphics PM init */
	MCHBAR32(0x5500) = 0x00100001;
}

void northbridge_enable(pci_dev_t dev)
{
#if CONFIG_HAVE_ACPI_RESUME
	switch (pci_read_config32(dev, SKPAD)) {
	case 0xcafebabe:
		debug("Normal boot.\n");
		apci_set_slp_type(0);
		break;
	case 0xcafed00d:
		debug("S3 Resume.\n");
		apci_set_slp_type(3);
		break;
	default:
		debug("Unknown boot method, assuming normal.\n");
		apci_set_slp_type(0);
		break;
	}
#endif
}

#if 0
static struct pci_operations intel_pci_ops = {
	.set_subsystem    = intel_set_subsystem,
};

static struct device_operations mc_ops = {
	.read_resources   = mc_read_resources,
	.set_resources    = mc_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.init             = northbridge_init,
	.enable           = northbridge_enable,
	.scan_bus         = 0,
	.ops_pci          = &intel_pci_ops,
};

static const struct pci_driver mc_driver_0100 __pci_driver = {
	.ops    = &mc_ops,
	.vendor = PCI_VENDOR_ID_INTEL,
	.device = 0x0100,
};

static const struct pci_driver mc_driver __pci_driver = {
	.ops    = &mc_ops,
	.vendor = PCI_VENDOR_ID_INTEL,
	.device = 0x0104, /* Sandy bridge */
};

static const struct pci_driver mc_driver_1 __pci_driver = {
	.ops    = &mc_ops,
	.vendor = PCI_VENDOR_ID_INTEL,
	.device = 0x0154, /* Ivy bridge */
};

static void cpu_bus_init(device_t dev)
{
	initialize_cpus(dev->link_list);
	/* Enable ROM caching if option was selected. */
	x86_mtrr_enable_rom_caching();
}

static void cpu_bus_noop(device_t dev)
{
}

static struct device_operations cpu_bus_ops = {
	.read_resources   = cpu_bus_noop,
	.set_resources    = cpu_bus_noop,
	.enable_resources = cpu_bus_noop,
	.init             = cpu_bus_init,
	.scan_bus         = 0,
};

static void enable_dev(device_t dev)
{
	/* Set the operations if it is a special bus type */
	if (dev->path.type == DEVICE_PATH_DOMAIN) {
		dev->ops = &pci_domain_ops;
	} else if (dev->path.type == DEVICE_PATH_CPU_CLUSTER) {
		dev->ops = &cpu_bus_ops;
	}
}

struct chip_operations northbridge_intel_sandybridge_ops = {
	CHIP_NAME("Intel i7 (SandyBridge/IvyBridge) integrated Northbridge")
	.enable_dev = enable_dev,
};
#endif
