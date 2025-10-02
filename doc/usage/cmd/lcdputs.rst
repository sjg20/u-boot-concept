.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: lcdputs (command)

lcdputs command
===============

Synopsis
--------

::

    lcdputs <string>

Description
-----------

The lcdputs command prints a string to the video framebuffer at the current
cursor position.

string
    Text string to display on the video console

Examples
--------

Print a simple string::

    => lcdputs "Hello World"

Combine with setcurs to position text::

    => setcurs 10 5
    => lcdputs "Positioned text"

Print multiple lines::

    => setcurs 0 0
    => lcdputs "Line 1"
    => setcurs 0 1
    => lcdputs "Line 2"

Configuration
-------------

The lcdputs command is available if CONFIG_CMD_VIDEO=y.

See also
--------

* :doc:`setcurs` - set cursor position

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.
