# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2025 Canonical Ltd
# Written by Simon Glass <simon.glass@canonical.com>

obj-y += ulib_test.o

# Link with the shared library
HOSTCFLAGS_ulib_test.o += -I$(srctree)/arch/sandbox/include
HOSTLDLIBS_ulib_test += -L$(obj)/../.. -lu-boot -Wl,-rpath,$(obj)/../..

# Ensure shared library is built first
$(obj)/ulib_test: $(obj)/../../libu-boot.so
