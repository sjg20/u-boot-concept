#!/bin/sh
# Copyright 2023 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Check that the board does not make use of the U_BOOT_DRVINFO() macro.
# Devicetree should be used instead. Where agreement proves impossible on that,
# a workaround can be added in dm_
#
# Usage
#    check-drv.sh <path to u-boot ELF> <path to .config> <path to allowlist file>
#
# For example:
#   scripts/check-drv.sh b//u-boot.map drv_allowlist.txt
#
# Exit code is 0 if OK, 3 if the macro is present, as above

set -e
set -u

PROG_NAME="${0##*/}"

usage() {
	echo "$PROG_NAME <path to .map> <path to .config> <path to allowlist file>"
	exit 1
}

[ $# -ge 2 ] || usage

map_path="$1"
cfg_path="$2"
allowlist="$3"

sys_config="$(sed -n 's/CONFIG_SYS_CONFIG_NAME="\(.*\)"$/\1/p' "${cfg_path}")"

if grep -q _u_boot_list_2_driver_info_2_ "${map_path}"; then
	echo >&2 "This board uses U_BOOT_DRVINFO() outside SPL."
	echo >&2 "Please move ${sys_config} to dm_"
	if grep -q "${sys_config}" "${allowlist}"; then
		exit 0
	fi
	exit 3
fi
