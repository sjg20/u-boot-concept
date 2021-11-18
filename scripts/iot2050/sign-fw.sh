#!/bin/sh

if [ -z "$1" ]; then
	echo "Usage: $0 KEY"
	exit 1
fi

mv spl/u-boot-spl-nodtb.bin spl/u-boot-spl-nodtb.bin.tmp
source/tools/k3_gen_x509_cert.sh -b spl/u-boot-spl-nodtb.bin.tmp -o spl/u-boot-spl-nodtb.bin -l 0x80080000 -k $1 -t $(dirname $1)/x509-sysfw-template.txt
rm spl/u-boot-spl-nodtb.bin.tmp

mv spl/dts/k3-am65-iot2050-spl.dtb spl/dts/k3-am65-iot2050-spl.dtb.tmp
source/tools/k3_gen_x509_cert.sh -b spl/dts/k3-am65-iot2050-spl.dtb.tmp -o spl/dts/k3-am65-iot2050-spl.dtb -k $1 -t $(dirname $1)/x509-sysfw-template.txt
rm spl/dts/k3-am65-iot2050-spl.dtb.tmp

tools/mkimage -f u-boot-spl-k3.its -o sha256,rsa4096 tispl.bin

source/tools/binman/binman replace -i flash.bin -f tispl.bin blob@0x080000

tools/mkimage -G $1 -r -o sha256,rsa4096 -F fit@0x280000.fit
source/tools/binman/binman replace -i flash.bin -f fit@0x280000.fit fit@0x280000
