// SPDX-License-Identifier: GPL-2.0+
/*
 *  EFI variable runtime access
 *
 *  Copyright (c) 2019 Linaro Limited, Author: AKASHI Takahiro
 */

#include <efi_loader.h>
#include <malloc.h>
#include <search.h>

/*
 * runtime version of APIs
 * We only support read-only variable access.
 * The table is in U-Boot's hash table format, but has its own
 * env_entry_node structure for specific use.
 *
 * Except for efi_freeze_variable_table(), which is to be called in
 * exit_boot_services(), all the functions and data below must be
 * placed in either RUNTIME_SERVICES_CODE or RUNTIME_SERVICES_DATA.
 */
typedef struct env_entry_node {
	unsigned int used;	/* hash value; 0 for not used */
	size_t name;		/* name offset from itself */
	efi_guid_t vendor;
	u32 attributes;
	size_t data;		/* data offset from itself */
	size_t data_size;
} _ENTRY;

static inline u16 *entry_name(struct env_entry_node *e)
{
	return (void *)e + e->name;
}

static inline u16 *entry_data(struct env_entry_node *e)
{
	return (void *)e + e->data;
}

static struct hsearch_data *efi_variable_table __efi_runtime_data;

static size_t __efi_runtime u16_strlen_runtime(const u16 *s1)
{
	size_t n = 0;

	while (*s1) {
		n++;
		s1++;
	}

	return n;
}

static int __efi_runtime memcmp_runtime(const void *m1, const void *m2,
					size_t n)
{
	while (n && *(u8 *)m1 == *(u8 *)m2) {
		n--;
		m1++;
		m2++;
	}

	if (n)
		return *(u8 *)m1 - *(u8 *)m2;

	return 0;
}

static void __efi_runtime memcpy_runtime(void *m1, const void *m2, size_t n)
{
	for (; n; n--, m1++, m2++)
		*(u8 *)m1 = *(u8 *)m2;
}

static int __efi_runtime efi_cmpkey(struct env_entry_node *e, const u16 *name,
				    const efi_guid_t *vendor)
{
	size_t name_len;

	name_len = u16_strlen_runtime(entry_name(e));

	/* return zero if matched */
	return name_len != u16_strlen_runtime(name) ||
	       memcmp_runtime(entry_name(e), name, name_len * 2) ||
	       memcmp_runtime(e->vendor.b, vendor->b, sizeof(vendor));
}

/* simplified and slightly different version of hsearch_r() */
static int __efi_runtime hsearch_runtime(const u16 *name,
					 const efi_guid_t *vendor,
					 enum env_action action,
					 struct env_entry_node **retval,
					 struct hsearch_data *htab)
{
	unsigned int hval;
	unsigned int count;
	unsigned int len;
	unsigned int idx, new;

	/* Compute an value for the given string. */
	len = u16_strlen_runtime(name);
	hval = len;
	count = len;
	while (count-- > 0) {
		hval <<= 4;
		hval += name[count];
	}

	/*
	 * First hash function:
	 * simply take the modulo but prevent zero.
	 */
	hval %= htab->size;
	if (hval == 0)
		++hval;

	/* The first index tried. */
	new = -1; /* not found */
	idx = hval;

	if (htab->table[idx].used) {
		/*
		 * Further action might be required according to the
		 * action value.
		 */
		unsigned int hval2;

		if (htab->table[idx].used == hval &&
		    !efi_cmpkey(&htab->table[idx], name, vendor)) {
			if (action == ENV_FIND) {
				*retval = &htab->table[idx];
				return idx;
			}
			/* we don't need to support overwrite */
			return -1;
		}

		/*
		 * Second hash function:
		 * as suggested in [Knuth]
		 */
		hval2 = 1 + hval % (htab->size - 2);

		do {
			/*
			 * Because SIZE is prime this guarantees to
			 * step through all available indices.
			 */
			if (idx <= hval2)
				idx = htab->size + idx - hval2;
			else
				idx -= hval2;

			/*
			 * If we visited all entries leave the loop
			 * unsuccessfully.
			 */
			if (idx == hval)
				break;

			/* If entry is found use it. */
			if (htab->table[idx].used == hval &&
			    !efi_cmpkey(&htab->table[idx], name, vendor)) {
				if (action == ENV_FIND) {
					*retval = &htab->table[idx];
					return idx;
				}
				/* we don't need to support overwrite */
				return -1;
			}
		} while (htab->table[idx].used);

		if (!htab->table[idx].used)
			new = idx;
	} else {
		new = idx;
	}

	/*
	 * An empty bucket has been found.
	 * The following code should never be executed after
	 * exit_boot_services()
	 */
	if (action == ENV_ENTER) {
		/*
		 * If table is full and another entry should be
		 * entered return with error.
		 */
		if (htab->filled == htab->size) {
			*retval = NULL;
			return 0;
		}

		/* Create new entry */
		htab->table[new].used = hval;
		++htab->filled;

		/* return new entry */
		*retval = &htab->table[new];
		return 1;
	}

	*retval = NULL;
	return 0;
}

/* from lib/hashtable.c */
static inline int isprime(unsigned int number)
{
	/* no even number will be passed */
	unsigned int div = 3;

	while (div * div < number && number % div != 0)
		div += 2;

	return number % div != 0;
}

efi_status_t efi_freeze_variable_table(void)
{
	int var_num = 0;
	size_t var_data_size = 0;
	u16 *name;
	efi_uintn_t name_buf_len, name_len;
	efi_guid_t vendor;
	u32 attributes;
	u8 *mem_pool, *var_buf = NULL;
	size_t table_size, var_size, var_buf_size;
	struct env_entry_node *new = NULL;
	efi_status_t ret;

	/* phase-1 loop */
	name_buf_len = 128;
	name = malloc(name_buf_len);
	if (!name)
		return EFI_OUT_OF_RESOURCES;
	name[0] = 0;
	for (;;) {
		name_len = name_buf_len;
		ret = EFI_CALL(efi_get_next_variable_name(&name_len, name,
							  &vendor));
		if (ret == EFI_NOT_FOUND) {
			break;
		} else if (ret == EFI_BUFFER_TOO_SMALL) {
			u16 *buf;

			name_buf_len = name_len;
			buf = realloc(name, name_buf_len);
			if (!buf) {
				free(name);
				return EFI_OUT_OF_RESOURCES;
			}
			name = buf;
			name_len = name_buf_len;
			ret = EFI_CALL(efi_get_next_variable_name(&name_len,
								  name,
								  &vendor));
		}

		if (ret != EFI_SUCCESS)
			return ret;

		var_size = 0;
		ret = EFI_CALL(efi_get_variable(name, &vendor, &attributes,
						&var_size, NULL));
		if (ret != EFI_BUFFER_TOO_SMALL)
			return ret;

		if (!(attributes & EFI_VARIABLE_RUNTIME_ACCESS))
			continue;

		var_num++;
		var_data_size += (u16_strlen_runtime(name) + 1) * sizeof(u16);
		var_data_size += var_size;
		/* mem_pool must 2-byte aligned for u16 variable name */
		if (var_data_size & 0x1)
			var_data_size++;
	}

	/*
	 * total of entries in hash table must be a prime number.
	 * The logic below comes from lib/hashtable.c
	 */
	var_num |= 1;               /* make odd */
	while (!isprime(var_num))
		var_num += 2;

	/* We need table[var_num] for hsearch_runtime algo */
	table_size = sizeof(*efi_variable_table)
			+ sizeof(struct env_entry_node) * (var_num + 1)
			+ var_data_size;
	ret = efi_allocate_pool(EFI_RUNTIME_SERVICES_DATA,
				table_size, (void **)&efi_variable_table);
	if (ret != EFI_SUCCESS)
		return ret;

	efi_variable_table->size = var_num;
	efi_variable_table->table = (void *)efi_variable_table
					+ sizeof(*efi_variable_table);
	mem_pool = (u8 *)efi_variable_table->table
			+ sizeof(struct env_entry_node) * (var_num + 1);

	var_buf_size = 128;
	var_buf = malloc(var_buf_size);
	if (!var_buf) {
		ret = EFI_OUT_OF_RESOURCES;
		goto err;
	}

	/* phase-2 loop */
	name[0] = 0;
	name_len = name_buf_len;
	for (;;) {
		name_len = name_buf_len;
		ret = EFI_CALL(efi_get_next_variable_name(&name_len, name,
							  &vendor));
		if (ret == EFI_NOT_FOUND)
			break;
		else if (ret != EFI_SUCCESS)
			goto err;

		var_size = var_buf_size;
		ret = EFI_CALL(efi_get_variable(name, &vendor, &attributes,
						&var_size, var_buf));
		if (ret == EFI_BUFFER_TOO_SMALL) {
			free(var_buf);
			var_buf_size = var_size;
			var_buf = malloc(var_buf_size);
			if (!var_buf) {
				ret = EFI_OUT_OF_RESOURCES;
				goto err;
			}
			ret = EFI_CALL(efi_get_variable(name, &vendor,
							&attributes,
							&var_size, var_buf));
		}
		if (ret != EFI_SUCCESS)
			goto err;

		if (!(attributes & EFI_VARIABLE_RUNTIME_ACCESS))
			continue;

		if (hsearch_runtime(name, &vendor, ENV_ENTER, &new,
				    efi_variable_table) <= 0) {
			/* This should not happen */
			ret = EFI_INVALID_PARAMETER;
			goto err;
		}

		/* allocate space from RUNTIME DATA */
		name_len = (u16_strlen_runtime(name) + 1) * sizeof(u16);
		memcpy_runtime(mem_pool, name, name_len);
		new->name = mem_pool - (u8 *)new; /* offset */
		mem_pool += name_len;

		memcpy_runtime(&new->vendor.b, &vendor.b, sizeof(vendor));

		new->attributes = attributes;

		memcpy_runtime(mem_pool, var_buf, var_size);
		new->data = mem_pool - (u8 *)new; /* offset */
		new->data_size = var_size;
		mem_pool += var_size;

		/* mem_pool must 2-byte aligned for u16 variable name */
		if ((uintptr_t)mem_pool & 0x1)
			mem_pool++;
	}
#ifdef DEBUG
	name[0] = 0;
	name_len = name_buf_len;
	for (;;) {
		name_len = name_buf_len;
		ret = efi_get_next_variable_name_runtime(&name_len, name,
							 &vendor);
		if (ret == EFI_NOT_FOUND)
			break;
		else if (ret != EFI_SUCCESS)
			goto err;

		var_size = var_buf_size;
		ret = efi_get_variable_runtime(name, &vendor, &attributes,
					       &var_size, var_buf);
		if (ret != EFI_SUCCESS)
			goto err;

		printf("%ls_%pUl:\n", name, &vendor);
		printf("    attributes: 0x%x\n", attributes);
		printf("    value (size: 0x%lx)\n", var_size);
	}
#endif
	ret = EFI_SUCCESS;

err:
	free(name);
	free(var_buf);
	if (ret != EFI_SUCCESS && efi_variable_table) {
		efi_free_pool(efi_variable_table);
		efi_variable_table = NULL;
	}

	return ret;
}

efi_status_t __efi_runtime EFIAPI
efi_get_variable_runtime(u16 *variable_name, const efi_guid_t *vendor,
			 u32 *attributes, efi_uintn_t *data_size, void *data)
{
	struct env_entry_node *new;

	if (!variable_name || !vendor || !data_size)
		return EFI_EXIT(EFI_INVALID_PARAMETER);

	if (hsearch_runtime(variable_name, vendor, ENV_FIND, &new,
			    efi_variable_table) <= 0)
		return EFI_NOT_FOUND;

	if (attributes)
		*attributes = new->attributes;
	if (*data_size < new->data_size) {
		*data_size = new->data_size;
		return EFI_BUFFER_TOO_SMALL;
	}

	*data_size = new->data_size;
	memcpy_runtime(data, entry_data(new), new->data_size);

	return EFI_SUCCESS;
}

static int prev_idx __efi_runtime_data;

efi_status_t __efi_runtime EFIAPI
efi_get_next_variable_name_runtime(efi_uintn_t *variable_name_size,
				   u16 *variable_name,
				   const efi_guid_t *vendor)
{
	struct env_entry_node *e;
	u16 *name;
	efi_uintn_t name_size;

	if (!variable_name_size || !variable_name || !vendor)
		return EFI_INVALID_PARAMETER;

	if (variable_name[0]) {
		/* sanity check for previous variable */
		if (prev_idx < 0)
			return EFI_INVALID_PARAMETER;

		e = &efi_variable_table->table[prev_idx];
		if (!e->used || efi_cmpkey(e, variable_name, vendor))
			return EFI_INVALID_PARAMETER;
	} else {
		prev_idx = -1;
	}

	/* next variable */
	while (++prev_idx <= efi_variable_table->size) {
		e = &efi_variable_table->table[prev_idx];
		if (e->used)
			break;
	}
	if (prev_idx > efi_variable_table->size)
		return EFI_NOT_FOUND;

	name = entry_name(e);
	name_size = (u16_strlen_runtime(name) + 1)
			* sizeof(u16);
	if (*variable_name_size < name_size) {
		*variable_name_size = name_size;
		return EFI_BUFFER_TOO_SMALL;
	}

	memcpy_runtime(variable_name, name, name_size);
	memcpy_runtime((void *)&vendor->b, &e->vendor.b, sizeof(vendor));

	return EFI_SUCCESS;
}

/**
 * efi_set_variable_runtime() - runtime implementation of SetVariable()
 */
efi_status_t __efi_runtime EFIAPI
efi_set_variable_runtime(u16 *variable_name, const efi_guid_t *vendor,
			 u32 attributes, efi_uintn_t data_size,
			 const void *data)
{
	return EFI_UNSUPPORTED;
}

efi_status_t __efi_runtime EFIAPI efi_query_variable_info_runtime(
			u32 attributes,
			u64 *maximum_variable_storage_size,
			u64 *remaining_variable_storage_size,
			u64 *maximum_variable_size)
{
	return EFI_UNSUPPORTED;
}
