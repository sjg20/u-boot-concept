/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Core ACPI (Advanced Configuration and Power Interface) support
 *
 * Copyright 2019 Google LLC
 *
 * Modified from coreboot file acpigen.h
 */

#ifndef __ACPI_ACPIGEN_H
#define __ACPI_ACPIGEN_H

struct acpi_ctx;

char *acpigen_get_current(struct acpi_ctx *ctx);

void acpigen_emit_byte(struct acpi_ctx *ctx, unsigned char data);

void acpigen_emit_word(struct acpi_ctx *ctx, unsigned int data);

void acpigen_emit_dword(struct acpi_ctx *ctx, unsigned int data);

#endif
