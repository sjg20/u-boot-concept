#!/usr/bin/awk -f
# SPDX-License-Identifier: GPL-2.0
# Generate a C program that prints all the CONFIG options in the given file

BEGIN {
	print "/* C program to print out CONFIG options */"
	print "#include <common.h>"
	print "#include <asm-offsets.h>"
	print "#include <asm/global_data.h>"
	print ""
	print "#include <linux/kbuild.h>"

	print
	print "int main(int argc, char **argv)"
	print "{"
}

END {
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

# 	printf("\tprintf(\"%s ", $2)
	printf("\tDEFINE(%s, %s);\n", $2, $2)
	next;

	# The only variables we expect, other than ENV settings, are hex
	is_hex = 1
	if ($2 ~ /^(CONFIG_EXTRA_ENV_SETTINGS)/)
		is_hex = 0;

	# Cast hex values to unsigned long so we can cover everything
	# Example output: CONFIG_SYS_PBSIZE 0x414
	if (is_hex)
# 		printf("%s", %s", "%#lx", $2)
		printf("%s\\n\", (unsigned long)(%s)", "%#lx", $2)

	# Put quotes around string values
	# Example output: CONFIG_EXTRA_ENV_SETTINGS "stdin=serial,cros-ec-keyb"
	else
		printf("\\\"%s\\\"\\n\", %s", "%s", $2)
# 	printf(");  /* %s */\n", $3)
# 	printf(");")
}
