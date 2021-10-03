.. SPDX-License-Identifier: GPL-2.0+:

bootdevice command
==================

Synopis
-------

::

    bootdevice list [-p]      - list all available bootdevices (-p to probe)\n"
    bootdevice select <bm>    - select a bootdevice by name\n"
    bootdevice info [-p]      - show information about a bootdevice";

Description
-----------

The `` command is used to manage bootdevices. It can list available
bootdevices, select one and obtain information about it.

See :doc:`../develop/bootflow` for more information about bootdevices in
general.


bootdevice list
~~~~~~~~~~~~~~~

This lists available bootdevices

Scanning with -p causes the bootdevices to be probed. This happens automatically
when they are used.

The list looks something like this:

===  ======  ======  ========  =========================
Seq  Probed  Status  Uclass    Name
===  ======  ======  ========  =========================
  0   [ + ]      OK  mmc       mmc@7e202000.bootdevice
  1   [   ]      OK  mmc       sdhci@7e300000.bootdevice
  2   [   ]      OK  ethernet  smsc95xx_eth.bootdevice
===  ======  ======  ========  =========================


The fields are as follows:

Seq:
    Sequence number in the scan, used to reference the bootflow later

Probed:
    Shows a plus (+) if the device is probed, empty if not.

Status:
    Shows the status of the device. Typically this is `OK` meaning that there is
    no error. If you use -p and an error occurs when probing, then this shows
    the error number. You can look up Linux error codes to find the meaning of
    the number.

Uclass:
    Name of the media device's Uclass. This indicates the type of the parent
    device (e.g. MMC, Ethernet).

Name:
    Name of the bootdevice. This is generated from the media device appended
    with `.bootdevice`


bootdevice select
~~~~~~~~~~~~~~~~~

Use this to select a particular bootdevice. You can select it by the sequence
number or name, as shown in `bootdevice list`.

Once a bootdevice is selected, you can use `bootdevice info` to look at it or
`bootflow scan` to scan it.

If no bootdevice name or number is provided, then any existing bootdevice is
unselected.


bootdevice info
~~~~~~~~~~~~~~~

This shows information on the current bootdevice, with the format looking like
this:

=========  =======================
Name       mmc@7e202000.bootdevice
Sequence   0
Status     Probed
Uclass     mmc
Bootflows  1 (1 valid)
=========  =======================

Most of the information is the same as `bootdevice list` above. The new fields
are:

Device
    Name of the bootdevice

Status
    Shows `Probed` if the device is probed, `OK` if not. If -p is used and the
    device fails to probe, an error code is shown.

Bootflows
    Indicates the number of bootflows attached to the bootdevice. This is 0
    unless you have used 'bootflow scan' on the bootflow, or on all bootflows.


Example
-------

This example shows listing available bootdevice and getting information about
one of them::

   U-Boot> bootdevice list
   Seq  Probed  Status  Uclass    Name
   ---  ------  ------  --------  ------------------
     0   [ + ]      OK  mmc       mmc@7e202000.bootdevice
     1   [   ]      OK  mmc       sdhci@7e300000.bootdevice
     2   [   ]      OK  ethernet  smsc95xx_eth.bootdevice
   ---  ------  ------  --------  ------------------
   (3 devices)
   U-Boot> bootdevice sel 0
   U-Boot> bootflow scan
   U-Boot> bootdevice info
   Name:      mmc@7e202000.bootdevice
   Sequence:  0
   Status:    Probed
   Uclass:    mmc
   Bootflows: 1 (1 valid)


Return value
------------

The return value $? is always 0 (true).


