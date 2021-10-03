.. SPDX-License-Identifier: GPL-2.0+

Output files
============

This describes some of the output files produced by the U-Boot build.

u-boot.cfg
    Contains the various CONFIG options defined by the build, in C header
    format. This includes both Kconfig options and ad-hoc CONFIG options;
    anything that starts with `CONFIG_` is included.

    This file is useful for seeing what options the build uses. It is also used
    by U-Boot to check against the config whitelist file,
    `config_whitelist.txt`.

    Example::

        #define CONFIG_SYS_TIMER_COUNTS_DOWN
        #define CONFIG_ETH 1
        #define CONFIG_CMD_FAT 1
        #define CONFIG_BOOTM_NETBSD 1
        #define CONFIG_CMD_FDT 1
        ...
