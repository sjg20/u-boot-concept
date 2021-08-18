.. SPDX-License-Identifier: GPL-2.0+:

bootmethod command
==================

Synopis
-------

::

    bootmethod list [-p]      - list all available bootmethods (-p to probe)\n"
    bootmethod select <bm>    - select a bootmethod by name\n"
    bootmethod info [-p]      - show information about a bootmethod";

Description
-----------

The `` command is used to manage bootmethods. It can list available
bootmethods, select one and obtain information about it.

See :doc:`../develop/bootflow` for more information about bootmethods in
general.


bootmethod list
~~~~~~~~~~~~~~~

This lists available bootmethods

Scanning with -p causes the bootmethods to be probed. This happens automatically
when they are used.

The list looks something like this:

===  ======  ======  ========  =========================
Seq  Probed  Status  Uclass    Name
===  ======  ======  ========  =========================
  0   [ + ]      OK  mmc       mmc@7e202000.bootmethod
  1   [   ]      OK  mmc       sdhci@7e300000.bootmethod
  2   [   ]      OK  ethernet  smsc95xx_eth.bootmethod
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
    Name of the bootmethod. This is generated from the media device appended
    with `.bootmethod`


bootmethod select
~~~~~~~~~~~~~~~~~

Use this to select a particular bootmethod. You can select it by the sequence
number or name, as shown in `bootmethod list`.

Once a bootmethod is selected, you can use `bootmethod info` to look at it or
`bootflow scan` to scan it.

If no bootmethod name or number is provided, then any existing bootmethod is
unselected.


bootmethod info
~~~~~~~~~~~~~~~

This shows information on the current bootmethod, with the format looking like
this:

=========  =======================
Name       mmc@7e202000.bootmethod
Sequence   0
Status     Probed
Uclass     mmc
Bootflows  1 (1 valid)
=========  =======================

Most of the information is the same as `bootmethod list` above. The new fields
are:

Device
    Name of the bootmethod

Status
    Shows `Probed` if the device is probed, `OK` if not. If -p is used and the
    device fails to probe, an error code is shown.

Bootflows
    Indicates the number of bootflows attached to the bootmethod. This is 0
    unless you have used 'bootflow scan' on the bootflow, or on all bootflows.


Example
-------

This example shows listing available bootmethod and getting information about
one of them::

   U-Boot> bootmethod list
   Seq  Probed  Status  Uclass    Name
   ---  ------  ------  --------  ------------------
     0   [ + ]      OK  mmc       mmc@7e202000.bootmethod
     1   [   ]      OK  mmc       sdhci@7e300000.bootmethod
     2   [   ]      OK  ethernet  smsc95xx_eth.bootmethod
   ---  ------  ------  --------  ------------------
   (3 devices)
   U-Boot> bootmethod sel 0
   U-Boot> bootflow scan
   U-Boot> bootmethod info
   Name:      mmc@7e202000.bootmethod
   Sequence:  0
   Status:    Probed
   Uclass:    mmc
   Bootflows: 1 (1 valid)


Return value
------------

The return value $? is always 0 (true).


