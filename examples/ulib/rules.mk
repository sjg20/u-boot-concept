# SPDX-License-Identifier: GPL-2.0+
#
# Build rules for U-Boot library examples
#
# Copyright 2025 Canonical Ltd.
# Written by Simon Glass <simon.glass@canonical.com>

# Generate normal and statically linked binary names from progs variable
all_bins := $(foreach prog,$(progs),$(OUTDIR)/$(prog) \
	$(OUTDIR)/$(prog)_static)

# Default target builds both programs
all: $(all_bins)

# System headers rule for objects that need system APIs
$(foreach obj,$(sys-objs),$(eval $(OUTDIR)/$(obj): \
	$(EXAMPLE_DIR)/$(obj:.o=.c) | $(OUTDIR) ; \
	$$(CC) $$(CFLAGS) $$(SYSTEM_CFLAGS) -c -o $$@ $$<))

# Automatic build rules for all programs
$(foreach prog,$(progs),$(eval $(OUTDIR)/$(prog): \
	$$(addprefix $(OUTDIR)/,$$($(prog)_objs)) ; \
	$$(CC) $$(CFLAGS) -o $$@ $$^ $$(SHARED_LDFLAGS)))
$(foreach prog,$(progs),$(eval $(OUTDIR)/$(prog)_static: \
	$$(addprefix $(OUTDIR)/,$$($(prog)_objs)) ; \
	$$(CC) $$(CFLAGS) -o $$@ $$^ $$(STATIC_LDFLAGS)))

# Create the output directory if it doesn't exist
$(OUTDIR):
	@mkdir -p $@

# Default rule: compile with U-Boot headers
$(OUTDIR)/%.o: $(EXAMPLE_DIR)/%.c | $(OUTDIR)
	$(CC) $(CFLAGS) $(UBOOT_CFLAGS) -c -o $@ $<

# The U-Boot library must be built before we can link against it
# Order-only prerequisites ensure libraries exist before linking
$(all_bins): | $(UBOOT_BUILD)/libu-boot.a $(UBOOT_BUILD)/libu-boot.so \
	$(OUTDIR)