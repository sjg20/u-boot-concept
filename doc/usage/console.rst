.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
..  (C) Copyright 2000

=======================
U-Boot console handling
=======================

HOW THE CONSOLE WORKS?
----------------------

At system startup U-Boot initializes a serial console. When U-Boot
relocates itself to RAM, all console drivers are initialized (they
will register all detected console devices to the system for further
use).

If not defined in the environment, the first input device is assigned
to the 'stdin' file, the first output one to 'stdout' and 'stderr'.

You can use the command "coninfo" to see all registered console
devices and their flags. You can assign a standard file (stdin,
stdout or stderr) to any device you see in that list simply by
assigning its name to the corresponding environment variable. For
example:

    setenv stdin serial		<- To use the serial input
    setenv stdout video		<- To use the video console

Do a simple "saveenv" to save the console settings in the environment
and get them working on the next startup, too.

HOW CAN I USE STANDARD FILE INTO THE SOURCES?
---------------------------------------------

You can use the following functions to access the console:

* STDOUT:
    putc	(to put a char to stdout)
    puts	(to put a string to stdout)
    printf	(to format and put a string to stdout)

* STDIN:
    tstc	(to test for the presence of a char in stdin)
    getc	(to get a char from stdin)

* STDERR:
    eputc	(to put a char to stderr)
    eputs	(to put a string to stderr)
    eprintf	(to format and put a string to stderr)

* FILE (can be 'stdin', 'stdout', 'stderr'):
    fputc	(like putc but redirected to a file)
    fputs	(like puts but redirected to a file)
    fprintf	(like printf but redirected to a file)
    ftstc	(like tstc but redirected to a file)
    fgetc	(like getc but redirected to a file)

Remember that all FILE-related functions CANNOT be used before
U-Boot relocation (done in 'board_init_r' in `arch/*/lib/board.c`).
