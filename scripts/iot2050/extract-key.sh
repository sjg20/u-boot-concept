#!/bin/sh

if [ -z "$1" ]; then
	echo "Usage: $0 KEYDIR"
	exit 1
fi

TMP_KEY=$(mktemp XXXXXXXX.dtb)
dtc -I dts -o $TMP_KEY <<EOF
/dts-v1/;
/ {
};
EOF

tools/fdt_add_pubkey -a sha256,rsa4096 -k $1 -n custMpk -r conf $TMP_KEY
dtc -I dtb $TMP_KEY -O dts -o custMpk.dtsi
sed -i '/^\/dts-v1\/;/d' custMpk.dtsi

rm -f $TMP_KEY
