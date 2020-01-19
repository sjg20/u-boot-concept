/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Based on acpi.c from coreboot
 *
 * Copyright (C) 2015, Saket Sinha <saket.sinha89@gmail.com>
 * Copyright (C) 2016, Bin Meng <bmeng.cn@gmail.com>
 */

#ifndef __ASM_ACPI_TABLE_H__
#define __ASM_ACPI_TABLE_H__

#ifndef __ACPI__

struct acpi_facs;
struct acpi_fadt;
struct acpi_global_nvs;
struct acpi_madt_ioapic;
struct acpi_madt_irqoverride;
struct acpi_madt_lapic_nmi;
struct acpi_mcfg_mmconfig;
struct acpi_table_header;

/* These can be used by the target port */

void acpi_fill_header(struct acpi_table_header *header, char *signature);
void acpi_create_fadt(struct acpi_fadt *fadt, struct acpi_facs *facs,
		      void *dsdt);
int acpi_create_madt_lapics(u32 current);
int acpi_create_madt_ioapic(struct acpi_madt_ioapic *ioapic, u8 id,
			    u32 addr, u32 gsi_base);
int acpi_create_madt_irqoverride(struct acpi_madt_irqoverride *irqoverride,
				 u8 bus, u8 source, u32 gsirq, u16 flags);
int acpi_create_madt_lapic_nmi(struct acpi_madt_lapic_nmi *lapic_nmi,
			       u8 cpu, u16 flags, u8 lint);
u32 acpi_fill_madt(u32 current);
int acpi_create_mcfg_mmconfig(struct acpi_mcfg_mmconfig *mmconfig, u32 base,
			      u16 seg_nr, u8 start, u8 end);
u32 acpi_fill_mcfg(u32 current);
u32 acpi_fill_csrt(u32 current);

int acpi_write_hpet(struct acpi_ctx *ctx, const struct udevice *dev);

int acpi_create_hpet(struct acpi_hpet *hpet);

int acpi_write_dbg2_pci_uart(struct acpi_ctx *ctx, struct udevice *dev,
			     uint access_size);

/**
 * acpi_create_gnvs() - Create a GNVS (Global Non Volatile Storage) table
 *
 * @gnvs: Table to fill in
 * @return 0 if OK, -ve on error
 */
int acpi_create_gnvs(struct acpi_global_nvs *gnvs);

ulong write_acpi_tables(ulong start);

int arch_read_sci_irq_select(void);
int arch_write_sci_irq_select(uint scis);
int arch_madt_sci_irq_polarity(int sci);
struct acpi_cstate *arch_get_cstate_map(size_t *entries);

int acpi_create_dmar_drhd(struct acpi_ctx *ctx, uint flags, uint segment,
			  u64 bar);
int acpi_create_dmar_rmrr(struct acpi_ctx *ctx, uint segment, u64 bar,
			  u64 limit);
void acpi_dmar_rmrr_fixup(struct acpi_ctx *ctx, void *base);
void acpi_dmar_drhd_fixup(struct acpi_ctx *ctx, void *base);

int acpi_create_dmar_ds_pci_br(struct acpi_ctx *ctx, pci_dev_t bdf);
int acpi_create_dmar_ds_pci(struct acpi_ctx *ctx, pci_dev_t bdf);
int acpi_create_dmar_ds_ioapic(struct acpi_ctx *ctx, uint enumeration_id,
			       pci_dev_t bdf);
int acpi_create_dmar_ds_msi_hpet(struct acpi_ctx *ctx, uint enumeration_id,
				 pci_dev_t bdf);
void acpi_fadt_common(struct acpi_fadt *fadt, struct acpi_facs *facs,
		      void *dsdt);
void intel_acpi_fill_fadt(struct acpi_fadt *fadt);
int intel_southbridge_write_acpi_tables(const struct udevice *dev,
					struct acpi_ctx *ctx);

#endif /* !__ACPI__ */

#endif /* __ASM_ACPI_TABLE_H__ */
