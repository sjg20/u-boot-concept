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
	if ($2 ~ /^(CONFIG_BOARDDIR|CONFIG_SYS_BAUDRATE_TABLE$)/)
		next;
	if ($2 ~ /\(/)
		next;
	if ($3 ~ /^[0-9]+$/)
		next;
	if ($3 ~ /^0x[0-9a-fA-F]+$/)
		next;
	if ($3 ~ /^"/)
		next;

	printf("\tDEFINE(%s, %s);\n", $2, $2)
}
