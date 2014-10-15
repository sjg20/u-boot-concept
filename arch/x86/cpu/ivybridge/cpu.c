/*
 * Copyright (c) 2014 Google, Inc
 * (C) Copyright 2008
 * Graeme Russ, graeme.russ@gmail.com.
 *
 * Some portions from coreboot src/mainboard/google/link/romstage.c
 * Copyright (C) 2007-2010 coresystems GmbH
 * Copyright (C) 2011 Google Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <errno.h>
#include <fdtdec.h>
#include <flash.h>
#include <netdev.h>
#include <ns16550.h>
#include <asm/lapic_def.h>
#include <asm/msr.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/mtrr.h>
#include <asm/pci.h>
#include <asm/post.h>
#include <asm/processor.h>
#include <asm/u-boot-x86.h>
#include <asm/arch/model_206ax.h>
#include <asm/arch/microcode.h>
#include <asm/arch/pch.h>
#include <asm/arch/sandybridge.h>

DECLARE_GLOBAL_DATA_PTR;

static void enable_lapic(void)
{
	uint32_t lo, hi;

	rdmsr(LAPIC_BASE_MSR, lo, hi);
	hi &= 0xffffff00;
	lo &= 0x000007ff;
	lo |= LAPIC_DEFAULT_BASE | (1 << 11);
	wrmsr(LAPIC_BASE_MSR, lo, hi);
}

#define GPIO_MODE_NATIVE	0
#define GPIO_MODE_GPIO		1
#define GPIO_MODE_NONE		1

#define GPIO_DIR_OUTPUT		0
#define GPIO_DIR_INPUT		1

#define GPIO_NO_INVERT		0
#define GPIO_INVERT		1

#define GPIO_LEVEL_LOW		0
#define GPIO_LEVEL_HIGH		1

#define GPIO_NO_BLINK		0
#define GPIO_BLINK		1

#define GPIO_RESET_PWROK	0
#define GPIO_RESET_RSMRST	1

struct pch_gpio_set1 {
	u32 gpio0 : 1;
	u32 gpio1 : 1;
	u32 gpio2 : 1;
	u32 gpio3 : 1;
	u32 gpio4 : 1;
	u32 gpio5 : 1;
	u32 gpio6 : 1;
	u32 gpio7 : 1;
	u32 gpio8 : 1;
	u32 gpio9 : 1;
	u32 gpio10 : 1;
	u32 gpio11 : 1;
	u32 gpio12 : 1;
	u32 gpio13 : 1;
	u32 gpio14 : 1;
	u32 gpio15 : 1;
	u32 gpio16 : 1;
	u32 gpio17 : 1;
	u32 gpio18 : 1;
	u32 gpio19 : 1;
	u32 gpio20 : 1;
	u32 gpio21 : 1;
	u32 gpio22 : 1;
	u32 gpio23 : 1;
	u32 gpio24 : 1;
	u32 gpio25 : 1;
	u32 gpio26 : 1;
	u32 gpio27 : 1;
	u32 gpio28 : 1;
	u32 gpio29 : 1;
	u32 gpio30 : 1;
	u32 gpio31 : 1;
} __attribute__ ((packed));

struct pch_gpio_set2 {
	u32 gpio32 : 1;
	u32 gpio33 : 1;
	u32 gpio34 : 1;
	u32 gpio35 : 1;
	u32 gpio36 : 1;
	u32 gpio37 : 1;
	u32 gpio38 : 1;
	u32 gpio39 : 1;
	u32 gpio40 : 1;
	u32 gpio41 : 1;
	u32 gpio42 : 1;
	u32 gpio43 : 1;
	u32 gpio44 : 1;
	u32 gpio45 : 1;
	u32 gpio46 : 1;
	u32 gpio47 : 1;
	u32 gpio48 : 1;
	u32 gpio49 : 1;
	u32 gpio50 : 1;
	u32 gpio51 : 1;
	u32 gpio52 : 1;
	u32 gpio53 : 1;
	u32 gpio54 : 1;
	u32 gpio55 : 1;
	u32 gpio56 : 1;
	u32 gpio57 : 1;
	u32 gpio58 : 1;
	u32 gpio59 : 1;
	u32 gpio60 : 1;
	u32 gpio61 : 1;
	u32 gpio62 : 1;
	u32 gpio63 : 1;
} __attribute__ ((packed));

struct pch_gpio_set3 {
	u32 gpio64 : 1;
	u32 gpio65 : 1;
	u32 gpio66 : 1;
	u32 gpio67 : 1;
	u32 gpio68 : 1;
	u32 gpio69 : 1;
	u32 gpio70 : 1;
	u32 gpio71 : 1;
	u32 gpio72 : 1;
	u32 gpio73 : 1;
	u32 gpio74 : 1;
	u32 gpio75 : 1;
} __attribute__ ((packed));

struct pch_gpio_map {
	struct {
		const struct pch_gpio_set1 *mode;
		const struct pch_gpio_set1 *direction;
		const struct pch_gpio_set1 *level;
		const struct pch_gpio_set1 *reset;
		const struct pch_gpio_set1 *invert;
		const struct pch_gpio_set1 *blink;
	} set1;
	struct {
		const struct pch_gpio_set2 *mode;
		const struct pch_gpio_set2 *direction;
		const struct pch_gpio_set2 *level;
		const struct pch_gpio_set2 *reset;
	} set2;
	struct {
		const struct pch_gpio_set3 *mode;
		const struct pch_gpio_set3 *direction;
		const struct pch_gpio_set3 *level;
		const struct pch_gpio_set3 *reset;
	} set3;
};

/* Configure GPIOs with mainboard provided settings */
void setup_pch_gpios(const struct pch_gpio_map *gpio);

/* get GPIO pin value */
int get_gpio(int gpio_num);
/*
 * get a number comprised of multiple GPIO values. gpio_num_array points to
 * the array of gpio pin numbers to scan, terminated by -1.
 */
unsigned get_gpios(const int *gpio_num_array);

static const struct pch_gpio_set1 pch_gpio_set1_mode = {
	.gpio0 = GPIO_MODE_GPIO,  /* NMI_DBG# */
	.gpio3 = GPIO_MODE_GPIO,  /* ALS_INT# */
	.gpio5 = GPIO_MODE_GPIO,  /* SIM_DET */
	.gpio7 = GPIO_MODE_GPIO,  /* EC_SCI# */
	.gpio8 = GPIO_MODE_GPIO,  /* EC_SMI# */
	.gpio9 = GPIO_MODE_GPIO,  /* RECOVERY# */
	.gpio10 = GPIO_MODE_GPIO, /* SPD vector D3 */
	.gpio11 = GPIO_MODE_GPIO, /* smbalert#, let's keep it initialized */
	.gpio12 = GPIO_MODE_GPIO, /* TP_INT# */
	.gpio14 = GPIO_MODE_GPIO, /* Touch_INT_L */
	.gpio15 = GPIO_MODE_GPIO, /* EC_LID_OUT# (EC_WAKE#) */
	.gpio21 = GPIO_MODE_GPIO, /* EC_IN_RW */
	.gpio24 = GPIO_MODE_GPIO, /* DDR3L_EN */
	.gpio28 = GPIO_MODE_GPIO, /* SLP_ME_CSW_DEV# */
};

static const struct pch_gpio_set1 pch_gpio_set1_direction = {
	.gpio0 = GPIO_DIR_INPUT,
	.gpio3 = GPIO_DIR_INPUT,
	.gpio5 = GPIO_DIR_INPUT,
	.gpio7 = GPIO_DIR_INPUT,
	.gpio8 = GPIO_DIR_INPUT,
	.gpio9 = GPIO_DIR_INPUT,
	.gpio10 = GPIO_DIR_INPUT,
	.gpio11 = GPIO_DIR_INPUT,
	.gpio12 = GPIO_DIR_INPUT,
	.gpio14 = GPIO_DIR_INPUT,
	.gpio15 = GPIO_DIR_INPUT,
	.gpio21 = GPIO_DIR_INPUT,
	.gpio24 = GPIO_DIR_OUTPUT,
	.gpio28 = GPIO_DIR_INPUT,
};

static const struct pch_gpio_set1 pch_gpio_set1_level = {
	.gpio1 = GPIO_LEVEL_HIGH,
	.gpio6 = GPIO_LEVEL_HIGH,
	.gpio24 = GPIO_LEVEL_LOW,
};

static const struct pch_gpio_set1 pch_gpio_set1_invert = {
	.gpio7 = GPIO_INVERT,
	.gpio8 = GPIO_INVERT,
	.gpio12 = GPIO_INVERT,
	.gpio14 = GPIO_INVERT,
	.gpio15 = GPIO_INVERT,
};

static const struct pch_gpio_set2 pch_gpio_set2_mode = {
	.gpio36 = GPIO_MODE_GPIO, /* W_DISABLE_L */
	.gpio41 = GPIO_MODE_GPIO, /* SPD vector D0 */
	.gpio42 = GPIO_MODE_GPIO, /* SPD vector D1 */
	.gpio43 = GPIO_MODE_GPIO, /* SPD vector D2 */
	.gpio57 = GPIO_MODE_GPIO, /* PCH_SPI_WP_D */
	.gpio60 = GPIO_MODE_GPIO, /* DRAMRST_CNTRL_PCH */
};

static const struct pch_gpio_set2 pch_gpio_set2_direction = {
	.gpio36 = GPIO_DIR_OUTPUT,
	.gpio41 = GPIO_DIR_INPUT,
	.gpio42 = GPIO_DIR_INPUT,
	.gpio43 = GPIO_DIR_INPUT,
	.gpio57 = GPIO_DIR_INPUT,
	.gpio60 = GPIO_DIR_OUTPUT,
};

static const struct pch_gpio_set2 pch_gpio_set2_level = {
	.gpio36 = GPIO_LEVEL_HIGH,
	.gpio60 = GPIO_LEVEL_HIGH,
};

static const struct pch_gpio_set3 pch_gpio_set3_mode = {
};

static const struct pch_gpio_set3 pch_gpio_set3_direction = {
};

static const struct pch_gpio_set3 pch_gpio_set3_level = {
};

static const struct pch_gpio_map link_gpio_map = {
	.set1 = {
		.mode      = &pch_gpio_set1_mode,
		.direction = &pch_gpio_set1_direction,
		.level     = &pch_gpio_set1_level,
		.invert    = &pch_gpio_set1_invert,
	},
	.set2 = {
		.mode      = &pch_gpio_set2_mode,
		.direction = &pch_gpio_set2_direction,
		.level     = &pch_gpio_set2_level,
	},
	.set3 = {
		.mode      = &pch_gpio_set3_mode,
		.direction = &pch_gpio_set3_direction,
		.level     = &pch_gpio_set3_level,
	},
};


#define MAX_GPIO_NUMBER 75 /* zero based */

void setup_pch_gpios(const struct pch_gpio_map *gpio)
{
	u16 gpiobase = pci_read_config16(PCH_LPC_DEV, GPIO_BASE) & 0xfffc;

	/* GPIO Set 1 */
	if (gpio->set1.level)
		outl(*((u32*)gpio->set1.level), gpiobase + GP_LVL);
	if (gpio->set1.mode)
		outl(*((u32*)gpio->set1.mode), gpiobase + GPIO_USE_SEL);
	if (gpio->set1.direction)
		outl(*((u32*)gpio->set1.direction), gpiobase + GP_IO_SEL);
	if (gpio->set1.reset)
		outl(*((u32*)gpio->set1.reset), gpiobase + GP_RST_SEL1);
	if (gpio->set1.invert)
		outl(*((u32*)gpio->set1.invert), gpiobase + GPI_INV);
	if (gpio->set1.blink)
		outl(*((u32*)gpio->set1.blink), gpiobase + GPO_BLINK);

	/* GPIO Set 2 */
	if (gpio->set2.level)
		outl(*((u32*)gpio->set2.level), gpiobase + GP_LVL2);
	if (gpio->set2.mode)
		outl(*((u32*)gpio->set2.mode), gpiobase + GPIO_USE_SEL2);
	if (gpio->set2.direction)
		outl(*((u32*)gpio->set2.direction), gpiobase + GP_IO_SEL2);
	if (gpio->set2.reset)
		outl(*((u32*)gpio->set2.reset), gpiobase + GP_RST_SEL2);

	/* GPIO Set 3 */
	if (gpio->set3.level)
		outl(*((u32*)gpio->set3.level), gpiobase + GP_LVL3);
	if (gpio->set3.mode)
		outl(*((u32*)gpio->set3.mode), gpiobase + GPIO_USE_SEL3);
	if (gpio->set3.direction)
		outl(*((u32*)gpio->set3.direction), gpiobase + GP_IO_SEL3);
	if (gpio->set3.reset)
		outl(*((u32*)gpio->set3.reset), gpiobase + GP_RST_SEL3);
}

static void enable_port80_on_lpc(void)
{
	pci_dev_t dev = PCI_BDF_CB(0, 0x1f, 0);

	/* Enable port 80 POST on LPC */
	pci_write_config32(dev, RCBA, DEFAULT_RCBA | 1);
	clrbits_le32(DEFAULT_RCBA + GCS, 4);
}

/*
 * Enable Prefetching and Caching.
 */
static void enable_spi_prefetch(void)
{
	pci_dev_t dev;
	u8 reg8;

	dev = PCI_BDF_CB(0, 0x1f, 0);

	reg8 = pci_read_config8(dev, 0xdc);
	reg8 &= ~(3 << 2);
	reg8 |= (2 << 2); /* Prefetching and Caching Enabled */
	pci_write_config8(dev, 0xdc, reg8);
}

#if 0
static void set_var_mtrr(
	unsigned reg, unsigned base, unsigned size, unsigned type)

{
	/* Bit Bit 32-35 of MTRRphysMask should be set to 1 */
	/* FIXME: It only support 4G less range */
	wrmsr(MTRRphysBase_MSR(reg), base | type, 0);
	wrmsr(MTRRphysMask_MSR(reg), ~(size - 1) | MTRRphysMaskValid,
	      (1 << (CONFIG_CPU_ADDR_BITS - 32)) - 1);
}

static void enable_rom_caching(void)
{
	disable_caches();
	set_var_mtrr(1, 0xffc00000, 4 << 20, MTRR_TYPE_WRPROT);
	enable_caches();

	/* Enable Variable MTRRs */
	wrmsr(MTRRdefType_MSR, 0x800, 0);
}
#endif

static int set_flex_ratio_to_tdp_nominal(void)
{
	unsigned low, high;
	u8 nominal_ratio;
	struct cpuid_result result;

	/* Minimum CPU revision for configurable TDP support */
	result = cpuid(1);
	if (result.eax < IVB_CONFIG_TDP_MIN_CPUID)
		return -EINVAL;

	/* Check for Flex Ratio support */
	rdmsr(MSR_FLEX_RATIO, low, high);
	if (!(low & FLEX_RATIO_EN))
		return -EINVAL;

	/* Check for >0 configurable TDPs */
	rdmsr(MSR_PLATFORM_INFO, low, high);
	if (((high >> 1) & 3) == 0)
		return -EINVAL;

	/* Use nominal TDP ratio for flex ratio */
	rdmsr(MSR_CONFIG_TDP_NOMINAL, low, high);
	nominal_ratio = low & 0xff;

	/* See if flex ratio is already set to nominal TDP ratio */
	rdmsr(MSR_FLEX_RATIO, low, high);
	if (((low >> 8) & 0xff) == nominal_ratio)
		return 0;

	/* Set flex ratio to nominal TDP ratio */
	low &= ~0xff00;
	low |= nominal_ratio << 8;
	low |= FLEX_RATIO_LOCK;
	wrmsr(MSR_FLEX_RATIO, low, high);

	/* Set flex ratio in soft reset data register bits 11:6.
	 * RCBA region is enabled in southbridge bootblock */
	clrsetbits_le32(&RCBA32(SOFT_RESET_DATA), 0x3f << 6,
			(nominal_ratio & 0x3f) << 6);

	/* Set soft reset control to use register value */
	setbits_le32(&RCBA32(SOFT_RESET_CTRL), 1);

	/* Issue warm reset, will be "CPU only" due to soft reset data */
	outb(0x0, 0xcf9);
	outb(0x6, 0xcf9);
	asm("hlt");

	/* Not reached */
	return -EINVAL;
}

static void set_spi_speed(void)
{
	u32 fdod;
	u8 ssfc;

	/* Observe SPI Descriptor Component Section 0 */
	RCBA32(0x38b0) = 0x1000;

	/* Extract the Write/Erase SPI Frequency from descriptor */
	fdod = RCBA32(0x38b4);
	fdod >>= 24;
	fdod &= 7;

	/* Set Software Sequence frequency to match */
	ssfc = RCBA8(0x3893);
	ssfc &= ~7;
	ssfc |= fdod;
	RCBA8(0x3893) = ssfc;
}

int arch_cpu_init(void)
{
	const void *blob = gd->fdt_blob;
	int node;
	int ret;

	timer_set_base(rdtsc());

	ret = x86_cpu_init_f();
	if (ret)
		return ret;

	node = fdtdec_next_compatible(blob, 0, COMPAT_INTEL_LPC);
	if (node < 0)
		return -ENOENT;
	ret = lpc_early_init(gd->fdt_blob, node, PCH_LPC_DEV);
	if (ret)
		return ret;

	enable_spi_prefetch();

	/* This is already done in start.S, but let's do it in C */
	enable_port80_on_lpc();

	set_spi_speed();

	ret = set_flex_ratio_to_tdp_nominal();
	if (ret)
		return ret;

	return 0;
}

static int enable_smbus(void)
{
	pci_dev_t dev;
	uint16_t value;

	/* Set the SMBus device statically. */
	dev = PCI_BDF_CB(0x0, 0x1f, 0x3);

	/* Check to make sure we've got the right device. */
	value = pci_read_config16(dev, 0x0);
	if (value != 0x8086) {
		post_code(0x83);
		printf("SMBus controller not found!");
		return -ENOSYS;
	}

	/* Set SMBus I/O base. */
	pci_write_config32(dev, SMB_BASE,
			   SMBUS_IO_BASE | PCI_BASE_ADDRESS_SPACE_IO);

	/* Set SMBus enable. */
	pci_write_config8(dev, HOSTC, HST_EN);

	/* Set SMBus I/O space enable. */
	pci_write_config16(dev, PCI_COMMAND, PCI_COMMAND_IO);

	/* Disable interrupt generation. */
	outb(0, SMBUS_IO_BASE + SMBHSTCTL);

	/* Clear any lingering errors, so transactions can run. */
	outb(inb(SMBUS_IO_BASE + SMBHSTSTAT), SMBUS_IO_BASE + SMBHSTSTAT);
	debug("SMBus controller enabled.\n");

	return 0;
}

#define PCH_EHCI1_TEMP_BAR0 0xe8000000
#define PCH_EHCI2_TEMP_BAR0 0xe8000400
#define PCH_XHCI_TEMP_BAR0  0xe8001000

/*
 * Setup USB controller MMIO BAR to prevent the
 * reference code from resetting the controller.
 *
 * The BAR will be re-assigned during device
 * enumeration so these are only temporary.
 */
static void enable_usb_bar(void)
{
	pci_dev_t usb0 = PCH_EHCI1_DEV;
	pci_dev_t usb1 = PCH_EHCI2_DEV;
	pci_dev_t usb3 = PCH_XHCI_DEV;
	u32 cmd;

	/* USB Controller 1 */
	pci_write_config32(usb0, PCI_BASE_ADDRESS_0,
			   PCH_EHCI1_TEMP_BAR0);
	cmd = pci_read_config32(usb0, PCI_COMMAND);
	cmd |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
	pci_write_config32(usb0, PCI_COMMAND, cmd);

	/* USB Controller 1 */
	pci_write_config32(usb1, PCI_BASE_ADDRESS_0,
			   PCH_EHCI1_TEMP_BAR0);
	cmd = pci_read_config32(usb1, PCI_COMMAND);
	cmd |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
	pci_write_config32(usb1, PCI_COMMAND, cmd);

	/* USB3 Controller */
	pci_write_config32(usb3, PCI_BASE_ADDRESS_0,
			   PCH_XHCI_TEMP_BAR0);
	cmd = pci_read_config32(usb3, PCI_COMMAND);
	cmd |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
	pci_write_config32(usb3, PCI_COMMAND, cmd);
}

static int report_bist_failure(void)
{
	if (gd->arch.bist != 0) {
		printf("BIST failed: %08x\n", gd->arch.bist);
		return -EFAULT;
	}

	return 0;
}

int print_cpuinfo(void)
{
	enum pei_boot_mode_t boot_mode = PEI_BOOT_NONE;
	char processor_name[CPU_MAX_NAME_LEN];
	const char *name;
	uint32_t pm1_cnt;
	uint16_t pm1_sts;
	int ret;

	/* Halt if there was a built in self test failure */
	ret = report_bist_failure();
	if (ret)
		return ret;

// 	enable_rom_caching();

	/* Enable GPIOs - needed for UART */
	pci_write_config32(PCH_LPC_DEV, GPIO_BASE, DEFAULT_GPIOBASE|1);
	pci_write_config32(PCH_LPC_DEV, GPIO_CNTL, 0x10);

	/* TODO: Do this later in the GPIO driver */
	setup_pch_gpios(&link_gpio_map);
	if (gd->arch.bist == 0)
		enable_lapic();

	ret = microcode_update_intel();
	if (ret && ret != -ENOENT && ret != -EEXIST)
		return ret;

	/* Enable upper 128bytes of CMOS */
	RCBA32(RC) = (1 << 2);

	/* TODO: cmos_post_init() */
	if (MCHBAR16(SSKPD) == 0xCAFE) {
		debug("soft reset detected\n");
		boot_mode = PEI_BOOT_SOFT_RESET;

		/* System is not happy after keyboard reset... */
		debug("Issuing CF9 warm reset\n");
		outb(0x6, 0xcf9);
		cpu_hlt();
	}

	/* Perform some early chipset initialization required
	 * before RAM initialization can work
	 */
	sandybridge_early_initialization(SANDYBRIDGE_MOBILE);

	/* Check PM1_STS[15] to see if we are waking from Sx */
	pm1_sts = inw(DEFAULT_PMBASE + PM1_STS);

	/* Read PM1_CNT[12:10] to determine which Sx state */
	pm1_cnt = inl(DEFAULT_PMBASE + PM1_CNT);

	if ((pm1_sts & WAK_STS) && ((pm1_cnt >> 10) & 7) == 5) {
#if CONFIG_HAVE_ACPI_RESUME
		debug("Resume from S3 detected.\n");
		boot_mode = PEI_BOOT_RESUME;
		/* Clear SLP_TYPE. This will break stage2 but
		 * we care for that when we get there.
		 */
		outl(pm1_cnt & ~(7 << 10), DEFAULT_PMBASE + PM1_CNT);
#else
		debug("Resume from S3 detected, but disabled.\n");
#endif
	} else {
		/* This is the fastest way to let users know
		 * the Intel CPU is now alive.
		 */
// 		google_chromeec_kbbacklight(100);
	}

	post_code(0x38);
	/* Enable SPD ROMs and DDR-III DRAM */
	ret = enable_smbus();
	if (ret)
		return ret;

	/* Prepare USB controller early in S3 resume */
	if (boot_mode == PEI_BOOT_RESUME)
		enable_usb_bar();

	gd->arch.pei_boot_mode = boot_mode;

	/* Print processor name */
	name = cpu_get_name(processor_name);
	printf("CPU:   %s\n", name);

	post_code(0x39);

	return 0;
}

void show_boot_progress(int val)
{
#if MIN_PORT80_KCLOCKS_DELAY
	/*
	 * Scale the time counter reading to avoid using 64 bit arithmetics.
	 * Can't use get_timer() here becuase it could be not yet
	 * initialized or even implemented.
	 */
	if (!gd->arch.tsc_prev) {
		gd->arch.tsc_base_kclocks = rdtsc() / 1000;
		gd->arch.tsc_prev = 0;
	} else {
		uint32_t now;

		do {
			now = rdtsc() / 1000 - gd->arch.tsc_base_kclocks;
		} while (now < (gd->arch.tsc_prev + MIN_PORT80_KCLOCKS_DELAY));
		gd->arch.tsc_prev = now;
	}
#endif
	post_code(val);
}

int board_eth_init(bd_t *bis)
{
	return pci_eth_init(bis);
}

#define MTRR_TYPE_WP          5

int board_final_cleanup(void)
{
	/*
	 * Un-cache the ROM so the kernel has one
	 * more MTRR available.
	 *
	 * Coreboot should have assigned this to the
	 * top available variable MTRR.
	 */
	u8 top_mtrr = (native_read_msr(MTRRcap_MSR) & 0xff) - 1;
	u8 top_type = native_read_msr(MTRRphysBase_MSR(top_mtrr)) & 0xff;

	/* Make sure this MTRR is the correct Write-Protected type */
	if (top_type == MTRR_TYPE_WP) {
		disable_caches();
		wrmsrl(MTRRphysBase_MSR(top_mtrr), 0);
		wrmsrl(MTRRphysMask_MSR(top_mtrr), 0);
		enable_caches();
	}

	/* TODO: Lock down ME and registers */

	return 0;
}

void panic_puts(const char *str)
{
	NS16550_t port = (NS16550_t)0x3f8;

	NS16550_init(port, 1);
	while (*str)
		NS16550_putc(port, *str++);
}
