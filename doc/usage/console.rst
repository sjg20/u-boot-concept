.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
..  (C) Copyright 2000

=======================
U-Boot console handling
=======================

Introduction
------------

At system-startup U-Boot initializes a serial console. When U-Boot
relocates itself to RAM, all console drivers are initialized (they
will register all detected console devices to the system for further
use).

If not defined in the environment, the first input device is assigned
to the 'stdin' file, the first output one to 'stdout' and 'stderr'.

You can use the command `coninfo` to see all registered console
devices and their flags. You can assign a standard file (stdin,
stdout or stderr) to any device you see in that list simply by
assigning its name to the corresponding environment variable. For
example::

    # Use the serial input
    setenv stdin serial

    # Use the video console
    setenv stdout vidconsole

Do a simple `saveenv` to save the console settings in the environment
and get them working on the next startup, too.

How to output text to the console
---------------------------------

You can use the following functions to access the console:

stdout
    - putc() - write a char to stdout
    - puts() - write a string to stdout
    - printf() - format and write a string to stdout

stdin
    - tstc() - test for the presence of a char in stdin
    - getchar() - get a char from stdin

stderr
    - eputc() - write a char to stderr
    - eputs() - write a string to stderr
    - eprintf() - format and write a string to stderr

file ('stdin', 'stdout' or 'stderr')
    - fputc() - write a char to a file
    - fputs() - write a string to a file
    - fprintf() - format and write a string to a file
    - ftstc() - test for the presence of a char in file
    - fgetc() - get a char from a file

Remember that FILE-related functions CANNOT be used before U-Boot relocation,
which is done in `board_init_r()`.
