#!/bin/sh
# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#
# Check that the ,config file provided does not try to disable OF_BOARD for
# boards that use CONFIG_OF_HAS_PRIOR_STAGE
#
# Usage
#    check-config.sh <path to .config> <path to whitelist file>
#
# For example:
#   scripts/check-config.sh b/chromebook_link/u-boot.cfg kconfig_whitelist.txt
#
# Exit code is 0 if OK, 3 if the .config is wrong, as above

set -e
set -u

PROG_NAME="${0##*/}"

usage() {
	echo "$PROG_NAME <path to .config> <path to whitelist file>"
	exit 1
}

[ $# -ge 2 ] || usage

path="$1"
whitelist="$2"

sys_config="$(sed -n 's/CONFIG_SYS_CONFIG_NAME="\(.*\)"$/\1/p' "${path}")"

if grep -q OF_HAS_PRIOR_STAGE=y "${path}"; then
	if ! grep -lq CONFIG_OF_BOARD=y "${path}"; then
		echo >&2 "This board uses a prior stage to provide the device tree."
		echo >&2 "Please enable CONFIG_OF_BOARD to ensure that it works correctly."
		if grep -q "${sys_config}" "${whitelist}"; then
			exit 0
		fi
		exit 3
	fi
fi
