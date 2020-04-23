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

#include <linux/types.h>

struct acpi_ctx;

/* ACPI Op/Prefix codes */
enum {
	ZERO_OP			= 0x00,
	ONE_OP			= 0x01,
	ALIAS_OP		= 0x06,
	NAME_OP			= 0x08,
	BYTE_PREFIX		= 0x0a,
	WORD_PREFIX		= 0x0b,
	DWORD_PREFIX		= 0x0c,
	STRING_PREFIX		= 0x0d,
	QWORD_PREFIX		= 0x0e,
	SCOPE_OP		= 0x10,
	BUFFER_OP		= 0x11,
	PACKAGE_OP		= 0x12,
	VARIABLE_PACKAGE_OP	= 0x13,
	METHOD_OP		= 0x14,
	EXTERNAL_OP		= 0x15,
	DUAL_NAME_PREFIX	= 0x2e,
	MULTI_NAME_PREFIX	= 0x2f,

	MUTEX_OP		= 0x01,
	EVENT_OP		= 0x01,
	SF_RIGHT_OP		= 0x10,
	SF_LEFT_OP		= 0x11,
	COND_REFOF_OP		= 0x12,
	CREATEFIELD_OP		= 0x13,
	LOAD_TABLE_OP		= 0x1f,
	LOAD_OP			= 0x20,
	STALL_OP		= 0x21,
	SLEEP_OP		= 0x22,
	ACQUIRE_OP		= 0x23,
	SIGNAL_OP		= 0x24,
	WAIT_OP			= 0x25,
	RST_OP			= 0x26,
	RELEASE_OP		= 0x27,
	FROM_BCD_OP		= 0x28,
	TO_BCD_OP		= 0x29,
	UNLOAD_OP		= 0x2a,
	REVISON_OP		= 0x30,
	DEBUG_OP		= 0x31,
	FATAL_OP		= 0x32,
	TIMER_OP		= 0x33,
	OPREGION_OP		= 0x80,
	FIELD_OP		= 0x81,
	DEVICE_OP		= 0x82,
	PROCESSOR_OP		= 0x83,
	POWER_RES_OP		= 0x84,
	THERMAL_ZONE_OP		= 0x85,
	INDEX_FIELD_OP		= 0x86,
	BANK_FIELD_OP		= 0x87,
	DATA_REGION_OP		= 0x88,

	EXT_OP_PREFIX		= 0x5b,
	ROOT_PREFIX		= 0x5c,
	PARENT_PREFIX		= 0x5d,
	LOCAL0_OP		= 0x60,
	LOCAL1_OP		= 0x61,
	LOCAL2_OP		= 0x62,
	LOCAL3_OP		= 0x63,
	LOCAL4_OP		= 0x64,
	LOCAL5_OP		= 0x65,
	LOCAL6_OP		= 0x66,
	LOCAL7_OP		= 0x67,
	ARG0_OP			= 0x68,
	ARG1_OP			= 0x69,
	ARG2_OP			= 0x6a,
	ARG3_OP			= 0x6b,
	ARG4_OP			= 0x6c,
	ARG5_OP			= 0x6d,
	ARG6_OP			= 0x6e,
	STORE_OP		= 0x70,
	REF_OF_OP		= 0x71,
	ADD_OP			= 0x72,
	CONCATENATE_OP		= 0x73,
	SUBTRACT_OP		= 0x74,
	INCREMENT_OP		= 0x75,
	DECREMENT_OP		= 0x76,
	MULTIPLY_OP		= 0x77,
	DIVIDE_OP		= 0x78,
	SHIFT_LEFT_OP		= 0x79,
	SHIFT_RIGHT_OP		= 0x7a,
	AND_OP			= 0x7b,
	NAND_OP			= 0x7c,
	OR_OP			= 0x7d,
	NOR_OP			= 0x7e,
	XOR_OP			= 0x7f,
	NOT_OP			= 0x80,
	FD_SHIFT_LEFT_BIT_OR	= 0x81,
	FD_SHIFT_RIGHT_BIT_OR	= 0x82,
	DEREF_OP		= 0x83,
	CONCATENATE_TEMP_OP	= 0x84,
	MOD_OP			= 0x85,
	NOTIFY_OP		= 0x86,
	SIZEOF_OP		= 0x87,
	INDEX_OP		= 0x88,
	MATCH_OP		= 0x89,
	CREATE_DWORD_OP		= 0x8a,
	CREATE_WORD_OP		= 0x8b,
	CREATE_BYTE_OP		= 0x8c,
	CREATE_BIT_OP		= 0x8d,
	OBJ_TYPE_OP		= 0x8e,
	CREATE_QWORD_OP		= 0x8f,
	LAND_OP			= 0x90,
	LOR_OP			= 0x91,
	LNOT_OP			= 0x92,
	LEQUAL_OP		= 0x93,
	LGREATER_OP		= 0x94,
	LLESS_OP		= 0x95,
	TO_BUFFER_OP		= 0x96,
	TO_DEC_STRING_OP	= 0x97,
	TO_HEX_STRING_OP	= 0x98,
	TO_INTEGER_OP		= 0x99,
	TO_STRING_OP		= 0x9c,
	CP_OBJ_OP		= 0x9d,
	MID_OP			= 0x9e,
	CONTINUE_OP		= 0x9f,
	IF_OP			= 0xa0,
	ELSE_OP			= 0xa1,
	WHILE_OP		= 0xa2,
	NOOP_OP			= 0xa3,
	RETURN_OP		= 0xa4,
	BREAK_OP		= 0xa5,
	COMMENT_OP		= 0xa9,
	BREAKPIONT_OP		= 0xcc,
	ONES_OP			= 0xff,
};

/**
 * acpigen_get_current() - Get the current ACPI code output pointer
 *
 * @ctx: ACPI context pointer
 * @return output pointer
 */
u8 *acpigen_get_current(struct acpi_ctx *ctx);

/**
 * acpigen_emit_byte() - Emit a byte to the ACPI code
 *
 * @ctx: ACPI context pointer
 * @data: Value to output
 */
void acpigen_emit_byte(struct acpi_ctx *ctx, uint data);

/**
 * acpigen_emit_word() - Emit a 16-bit word to the ACPI code
 *
 * @ctx: ACPI context pointer
 * @data: Value to output
 */
void acpigen_emit_word(struct acpi_ctx *ctx, uint data);

/**
 * acpigen_emit_dword() - Emit a 32-bit 'double word' to the ACPI code
 *
 * @ctx: ACPI context pointer
 * @data: Value to output
 */
void acpigen_emit_dword(struct acpi_ctx *ctx, uint data);

/**
 * acpigen_emit_stream() - Emit a stream of bytes
 *
 * @ctx: ACPI context pointer
 * @data: Data to output
 * @size: Size of data in bytes
 */
void acpigen_emit_stream(struct acpi_ctx *ctx, const char *data, int size);

/**
 * acpigen_emit_string() - Emit a string
 *
 * Emit a string with a nul terminator
 *
 * @ctx: ACPI context pointer
 * @str: String to output, or NULL for an empty string
 */
void acpigen_emit_string(struct acpi_ctx *ctx, const char *str);

/**
 * acpigen_write_len_f() - Write a 'forward' length placeholder
 *
 * This adds space for a length value in the ACPI stream and pushes the current
 * position (before the length) on the stack. After calling this you can write
 * some data and then call acpigen_pop_len() to update the length value.
 *
 * Usage:
 *
 *    acpigen_write_len_f() ------\
 *    acpigen_write...()          |
 *    acpigen_write...()          |
 *      acpigen_write_len_f() --\ |
 *      acpigen_write...()      | |
 *      acpigen_write...()      | |
 *      acpigen_pop_len() ------/ |
 *    acpigen_write...()          |
 *    acpigen_pop_len() ----------/
 *
 * @ctx: ACPI context pointer
 */
void acpigen_write_len_f(struct acpi_ctx *ctx);

/**
 * acpigen_pop_len() - Update the previously stacked length placeholder
 *
 * Call this after the data for the block gas been written. It updates the
 * top length value in the stack and pops it off.
 *
 * @ctx: ACPI context pointer
 */
void acpigen_pop_len(struct acpi_ctx *ctx);

/**
 * acpigen_write_package() - Start writing a package
 *
 * A package collects together a number of elements in the ACPI code. To write
 * a package use:
 *
 * acpigen_write_package(ctx, 3);
 * ...write things
 * acpigen_pop_len()
 *
 * If you don't know the number of elements in advance, acpigen_write_package()
 * returns a pointer to the value so you can update it later:
 *
 * char *num_elements = acpigen_write_package(ctx, 0);
 * ...write things
 * *num_elements += 1;
 * ...write things
 * *num_elements += 1;
 * acpigen_pop_len()
 *
 * @ctx: ACPI context pointer
 * @nr_el: Number of elements (0 if not known)
 * @returns pointer to the number of elements, which can be updated by the
 *	caller if needed
 */
char *acpigen_write_package(struct acpi_ctx *ctx, int nr_el);

/**
 * acpigen_write_integer() - Write an integer
 *
 * This writes an operation (BYTE_OP, WORD_OP, DWORD_OP, QWORD_OP depending on
 * the integer size) and an integer value. Note that WORD means 16 bits in ACPI.
 *
 * @ctx: ACPI context pointer
 * @data: Integer to write
 */
void acpigen_write_integer(struct acpi_ctx *ctx, u64 data);

/**
 * acpigen_write_string() - Write a string
 *
 * This writes a STRING_PREFIX followed by a nul-terminated string
 *
 * @ctx: ACPI context pointer
 * @str: String to write
 */
void acpigen_write_string(struct acpi_ctx *ctx, const char *str);

/**
 * acpigen_emit_namestring() - Emit an ACPI name
 *
 * This writes out an ACPI name or path in the required special format. It does
 * not add the NAME_OP prefix.
 *
 * @ctx: ACPI context pointer
 * @namepath: Name / path to emit
 */
void acpigen_emit_namestring(struct acpi_ctx *ctx, const char *namepath);

/**
 * acpigen_write_name() - Write out an ACPI name
 *
 * This writes out an ACPI name or path in the required special format with a
 * NAME_OP prefix.
 *
 * @ctx: ACPI context pointer
 * @namepath: Name / path to emit
 */
void acpigen_write_name(struct acpi_ctx *ctx, const char *namepath);

/**
 * acpigen_write_uuid() - Write a UUID
 *
 * This writes out a UUID in the format used by ACPI, with a BUFFER_OP prefix.
 *
 * @ctx: ACPI context pointer
 * @uuid: UUID to write in the form aabbccdd-eeff-gghh-iijj-kkllmmnnoopp
 * @return 0 if OK, -EINVAL if the format is incorrect
 */
int acpigen_write_uuid(struct acpi_ctx *ctx, const char *uuid);

#endif
