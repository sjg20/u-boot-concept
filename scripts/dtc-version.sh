#!/bin/sh
#
# dtc-version dtc_command min_version build_dtc
#
# Selects which version of dtc to use
#
# If the version of dtc_command is < min_version, prints build_dtc else
# prints dtc_command. The min_version is in the format MMmmpp where:
#
#   MM is the major version
#   mm is the minor version
#   pp is the patch level
#
# For example 010406 means 1.4.6
#

dtc="$1"
min_version="$2"
build_dtc="$3"

if [ ${#dtc} -eq 0 ]; then
	echo "Error: No dtc command specified."
	printf "Usage:\n\t$0 <dtc-command>\n"
	exit 1
fi

MAJOR=$($dtc -v | head -1 | awk '{print $NF}' | cut -d . -f 1)
MINOR=$($dtc -v | head -1 | awk '{print $NF}' | cut -d . -f 2)
PATCH=$($dtc -v | head -1 | awk '{print $NF}' | cut -d . -f 3 | cut -d - -f 1)

version="$(printf "%02d%02d%02d" $MAJOR $MINOR $PATCH)"
if test $version -lt $min_version; then \
	echo $build_dtc
else
	echo $dtc
fi
