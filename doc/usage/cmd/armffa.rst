.. SPDX-License-Identifier: GPL-2.0+:

armffa command
==============

Synopsis
--------

::

   armffa [sub-command] [arguments]

   sub-commands:

        getpart [partition UUID]

            lists the partition(s) info

        ping [partition ID]

            sends a data pattern to the specified partition

        devlist

            displays instance info of FF-A devices (the bus and its associated sandbox)

Description
-----------

armffa is a command showcasing how to use the FF-A driver and how to invoke its operations.

This provides a guidance to the client developers on how to call the FF-A bus interfaces.

The command also allows to gather secure partitions information and ping these  partitions.

The command is also helpful in testing the communication with secure partitions.

Example
-------

The following examples are run on Corstone-1000 platform with debug logs enabled.

* ping

::

   corstone1000# armffa ping 0x8003
   [FFA] endpoint ID is 0
   [FFA] Using 1 4KB page(s) for RX/TX buffers size
   [FFA] RX buffer at virtual address 00000000fdf48000
   [FFA] TX buffer at virtual address 00000000fdf4a000
   [FFA] RX/TX buffers mapped
   [FFA] Reading partitions data from the RX buffer
   [FFA] Partition ID 8001 : info cached
   [FFA] Partition ID 8002 : info cached
   [FFA] Partition ID 8003 : info cached
   [FFA] 3 partition(s) found and cached
   [FFA] SP response:
   [LSB]
   [FFA] 0xfffffffe
   [FFA] 0x0
   [FFA] 0x0
   [FFA] 0x0
   [FFA] 0x0

* ping (failure case)

::

   corstone1000# armffa ping 0x1234
   [FFA] Sending direct request error (-22)
   Command 'ping' failed: Error -22

* getpart

::

   corstone1000# armffa getpart 33d532ed-e699-0942-c09c-a798d9cd722d
   [FFA] Preparing for checking partitions count
   [FFA] Searching partitions using the provided UUID
   [FFA] No partition found. Querying framework ...
   [FFA] Reading partitions data from the RX buffer
   [FFA] Number of partition(s) matching the UUID: 1
   [FFA] Pre-allocating 1 partition(s) info structures
   [FFA] Preparing for filling partitions info
   [FFA] Searching partitions using the provided UUID
   [FFA] Partition ID 8003 matches the provided UUID
   [FFA] Partition: id = 0x8003 , exec_ctxt 0x1 , properties 0x3

* getpart (failure case)

::

    corstone1000# armffa getpart ed32d533-4209-99e6-2d72-cdd998a79cc0
    [FFA] Preparing for checking partitions count
    [FFA] Searching partitions using the provided UUID
    [FFA] No partition found. Querying framework ...
    [FFA] INVALID_PARAMETERS: Unrecognized UUID
    [FFA] Failure in querying partitions count (error code: -22)
    Command 'getpart' failed: Error -22

* devlist

::

   corstone1000# armffa devlist
   [FFA] FF-A uclass entries:
   [FFA] entry 0 - instance fdf40c50, ops fffc0408, plat 00000000

Configuration
-------------

The command is available if CONFIG_CMD_ARMFFA=y and CONFIG_ARM_FFA_TRANSPORT=y.

Return value
------------

The return value $? is 0 (true) on success and a negative error code on failure.
