.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: setcurs (command)

setcurs command
===============

Synopsis
--------

::

    setcurs <col> <row>

Description
-----------

The setcurs command sets the cursor position on the video console.

col
    Column position in hex, with 0 being the left side. Note that this is the
    text-column position, so the number of pixels per position depends on the
    font size.

row
    Row position in hex, with 0 being the top edge. Note that this is the
    text-row position, so the number of pixels per position depends on the
    font size.


Examples
--------

Set cursor to column 0x10, row 5::

    => setcurs 10 5

Move cursor to top left::

    => setcurs 0 0

Configuration
-------------

The setcurs command is available if CONFIG_CMD_VIDEO=y.

See also
--------

* :doc:`lcdputs` - print string on video framebuffer

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.
