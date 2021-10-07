#!/bin/sh
# SPDX-License-Identifier: GPL-2.0+
#
# script to add a certificate (efi-signature-list) to dtb blob

usage() {
	if [ -n "$*" ]; then
		echo "ERROR: $*"
	fi
	echo "Usage: "$(basename $0) " <esl file> <dtb file>"
}

if [ "$#" -ne 2 ]; then
	usage "Arguments missing"
	exit 1
fi

ESL=$1
DTB=$2
NEW_DTB=$(basename $DTB)_tmp
SIG=signature

cat << 'EOF' > $SIG.dts
/dts-v1/;
/plugin/;

&{/} {
    signature {
	    capsule-key = /incbin/("ESL");
    };
};
EOF

sed -in "s/ESL/$ESL/" $SIG.dts

dtc -@ -I dts -O dtb -o $SIG.dtbo $SIG.dts
fdtoverlay -i $DTB -o $NEW_DTB -v $SIG.dtbo
mv $NEW_DTB $DTB

#rm $SIG.dts $SIG.dtbo $NEW_DTB
