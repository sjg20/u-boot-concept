#!/usr/bin/awk -f
# SPDX-License-Identifier: GPL-2.0
# Generate a C program that uses all the CONFIG options in the given file
# This can then be compiled to assembler, which produces the values in the .S
# file. Then this is parsed by filechk_cfgv to obtain the literal values and
# generate a file containing those

BEGIN {
	print "/* C program which can be assembled to evaluate CONFIG options */"
	print "#include <common.h>"
	print "#include <asm-offsets.h>"
	print "#include <asm/global_data.h>"
	print "#include <generated/generic-asm-offsets.h>"
	print ""
	print "#include <linux/kbuild.h>"

	print
	print "int main(int argc, char **argv)"
	print "{"
}

END {
	print ""
	print "\treturn 0;"
	print "}"
}

/^#define (CONFIG_\w+)\s*(.*)$/ {
	if (length($3) == 0)
		next;

	# Ignore strange ones that don't have a single value
	if ($2 ~ /^(CONFIG_BOARDDIR|CONFIG_SYS_BAUDRATE_TABLE$)/)
		next;

	# Ignore macros with parameters
	if ($2 ~ /\(/)
		next;

	# Ignore values containing a comma, period, colon or brace, since we
        # won't be able to evaluate them anyway. These might be Ethernet MAC
        # addresses, like: #define CONFIG_ETHBASE 00:50:C2:13:6f:00
	if ($3 ~ /[,.:{}]/)
		next;

	# Ignore simple decimal integers, since they will appear in u-boot.cfg
	# and don't need any special handling
	if ($3 ~ /^[0-9]+$/)
		next;

	# Same with simple hex integers
	if ($3 ~ /^0x[0-9a-fA-F]+$/)
		next;

	# Ignore strings
	if ($3 ~ /^"/)
		next;

	# Ignore anything that seems to reference gd
	if ($3 ~ /gd->/)
		next;

	# Special cases for PPC / Freescale
	if ($3 ~ /get_serial_clock|get_phys_ccsrbar_addr_early/)
		next;

	# Special cases for PPC / Freescale which breaks all the rules and has a
	# lot of unmigrated coded
	if ($2 ~ /CONFIG_SYS_FSL|CONFIG_SYS_MPC|CONFIG_SYS_PCIE/)
		next;
	if ($2 ~ /CONFIG_SYS_CCSRBAR_PHYS|CONFIG_SYS_PAMU/)
		next;

	printf("\tDEFINE(%s, %s);\n", $2, $2)
}
