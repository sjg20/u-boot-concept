.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: video (command)

video command
=============

Synopsis
--------

::

    video setcursor <col> <row>
    video puts <string>

Description
-----------

The video command provides access to the video-console subsystem.

video setcursor
~~~~~~~~~~~~~~~

    video setcursor <col> <row>

Set the cursor position on the video console.

col
    Column position in hex, with 0 being the left side. Note that this is the
    text-column position, so the number of pixels per position depends on the
    font size.

row
    Row position in hex, with 0 being the top edge. Note that this is the
    text-row position, so the number of pixels per position depends on the
    font size.

video puts
~~~~~~~~~~

    video puts <string>

Write a string to the video console at the current cursor position.

string
    Text string to display

Examples
--------

Set cursor and print text::

    => video setcursor 10 5
    => video puts "Hello World"

Print at different positions::

    => video setcursor 0 0
    => video puts "Top left"
    => video setcursor 0 10
    => video puts "Line 16"

Configuration
-------------

The video command is available if CONFIG_CMD_VIDEO=y.

Return value
------------

The return value $? is 0 (true) on success, 1 (false) on failure.
