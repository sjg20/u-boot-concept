.. SPDX-License-Identifier: GPL-2.0+

Arm FF-A Driver
===============

Summary
-------

FF-A stands for Firmware Framework for Arm A-profile processors.

FF-A specifies interfaces that enable a pair of software sandboxes to
communicate with each other. A sandbox aka partition could
be a VM in the Normal or Secure world, an application in S-EL0, or a
Trusted OS in S-EL1.

This FF-A driver implements the interfaces to communicate with partitions in
the Secure world aka Secure partitions (SPs).

The driver specifically focuses on communicating with SPs that isolate portions
of EFI runtime services that must run in a protected environment which is
inaccessible by the Host OS or Hypervisor. Examples of such services are
set/get variables.

FF-A driver uses the SMC ABIs defined by the FF-A specification to:

- Discover the presence of SPs of interest
- Access an SP's service through communication protocols
  e.g. EFI MM communication protocol

At this stage of development the FF-A driver supports EFI boot time only.

Runtime support will be added in future developments.

FF-A and SMC specifications
-------------------------------------------

The current implementation of the driver relies on FF-A specification v1.0
and uses SMC32 calling convention which means using the first 32-bit data of the
Xn registers.

At this stage we only need the FF-A v1.0 features.

The driver has been tested with OP-TEE which supports SMC32 calling convention.

For more details please refer to the FF-A v1.0 spec:
https://documentation-service.arm.com/static/5fb7e8a6ca04df4095c1d65e?token=

Hypervisors are supported if they are configured to trap SMC calls.

The FF-A driver uses 64-bit registers as per SMCCCv1.2 specification.

For more details please refer to the SMC Calling Convention v1.2 spec:
https://documentation-service.arm.com/static/5f8edaeff86e16515cdbe4c6?token=

Supported hardware
--------------------------------

Aarch64 plaforms

Configuration
----------------------

CONFIG_ARM_FFA_TRANSPORT
    Enables the FF-A bus driver. Turn this on if you want to use FF-A
    communication.

FF-A ABIs under the hood
---------------------------------------

Invoking an FF-A ABI involves providing to the secure world/hypervisor the
expected arguments from the ABI.

The ABI arguments are stored in x0 to x7 registers. Then, an SMC instruction
is executed.

At the secure side level or hypervisor the ABI is handled at a higher exception
level and the arguments are read and processed.

The response is put back through x0 to x7 registers and control is given back
to the U-Boot FF-A driver (non-secure world).

The driver reads the response and processes it accordingly.

This methodology applies to all the FF-A ABIs in the driver.

FF-A bus discovery in U-Boot
-------------------------------------------

When CONFIG_ARM_FFA_TRANSPORT is enabled, the FF-A bus is considered as
an architecture feature and discovered using ARM_SMCCC_FEATURES mechanism.
This discovery mechanism is performed by the PSCI driver.

The PSCI driver comes with a PSCI device tree node which is the root node for all
architecture features including FF-A bus.

::

   => dm tree

    Class     Index  Probed  Driver                Name
   -----------------------------------------------------------
   ...
    firmware      0  [ + ]   psci                      |-- psci
    ffa                   0  [   ]   arm_ffa               |   `-- arm_ffa
   ...

The PSCI driver is bound to the PSCI device and when probed it tries to discover
the architecture features by calling a callback the features drivers provide.

In case of FF-A, the callback is ffa_bus_is_supported() which tries to discover the
FF-A framework by querying the FF-A framework version from secure world using
FFA_VERSION ABI. When discovery is successful, the ARM_SMCCC_FEATURES
mechanism creates a U-Boot device for the FF-A bus and binds the FF-A driver
with the device using device_bind_driver().

At this stage the FF-A bus is registered with the DM and can be interacted with using
the DM APIs.

Clients are able to probe then use the FF-A bus by calling the DM class searching APIs
(e.g: uclass_get_device_by_name). Please refer to the armffa command implementation
as an example of how to probe and interact with the FF-A driver.

When calling uclass_get_device_by_name() for example, the FF-A driver is probed
and ffa_probe() is called which performs the following:

    - allocating private data (priv) with devres
    - updating priv with discovery information
    - querying from secure world the u-boot endpoint ID
    - querying from secure world the supported features of FFA_RXTX_MAP
    - mapping the RX/TX buffers
    - querying from secure world all the partitions information

When one of the above actions fails, probing fails and the driver stays not active
and can be probed again if needed.

FF-A device destruction
-------------------------

When the FF-A device is removed by the DM, RX/TX buffers are automatically
unmapped and freed. Same happens when the device is unbound before being
removed first.

For example, at EFI efi_exit_boot_services() active devices are automatically removed
by dm_remove_devices_flags(DM_REMOVE_ACTIVE_ALL).

By consequence, the FF-A RX/TX are unmapped automatically.

Requirements for clients
-------------------------------------

When using the FF-A bus with EFI, clients must query the SPs they are looking for
during EFI boot time mode using the service UUID.

The RX/TX buffers are only available at EFI boot time. Querying partitions is
done at boot time and data is cached for future use.

RX/TX buffers should be unmapped before EFI runtime mode starts.
The driver provides a bus operation for that called rxtx_unmap() and this is done
automatically at efi_exit_boot_services().

If  RX/TX buffers created by U-Boot are not unmapped and by consequence becoming
available at EFI runtime, secure world will get confused about RX/TX buffers
ownership (U-Boot vs kernel).

When invoking FF-A direct messaging, clients should specify which ABI protocol
they want to use (32-bit vs 64-bit). Selecting the protocol means using
the 32-bit or 64-bit version of FFA_MSG_SEND_DIRECT_{REQ, RESP}.
The calling convention between U-Boot and the secure world stays the same: SMC32.

The bus driver layer
------------------------------

The driver comes on top of the SMCCC layer and is implemented in
drivers/firmware/arm-ffa/core.c

The driver provides the following features:

- Support for the 32-bit version of the following ABIs:

    - FFA_VERSION
    - FFA_ID_GET
    - FFA_FEATURES
    - FFA_PARTITION_INFO_GET
    - FFA_RXTX_UNMAP
    - FFA_RX_RELEASE
    - FFA_RUN
    - FFA_ERROR
    - FFA_SUCCESS
    - FFA_INTERRUPT
    - FFA_MSG_SEND_DIRECT_REQ
    - FFA_MSG_SEND_DIRECT_RESP

- Support for the 64-bit version of the following ABIs:

    - FFA_RXTX_MAP
    - FFA_MSG_SEND_DIRECT_REQ
    - FFA_MSG_SEND_DIRECT_RESP

- Processing the received data from the secure world/hypervisor and caching it

- Hiding from upper layers the FF-A protocol and registers details. Upper
  layers focus on exchanged data, the driver takes care of how to transport
  that to the secure world/hypervisor using FF-A

- The driver provides callbacks to be used by clients to access the FF-A bus:

    - partition_info_get
    - sync_send_receive
    - rxtx_unmap

- FF-A bus discovery makes sure FF-A framework is responsive and compatible
  with the driver

- FF-A bus can be compiled and used without EFI

The armffa command
-----------------------------------

armffa is an implementation defined command showcasing how to use the FF-A driver and how to invoke
its operations.

Please refer the command documentation at doc/usage/cmd/armffa.rst

Example of boot logs with FF-A enabled
--------------------------------------

For example, when using FF-A with Corstone-1000 the logs are as follows:

::

   U-Boot 2023.01 (Mar 07 2023 - 11:05:21 +0000) corstone1000 aarch64

   DRAM:  2 GiB
   [FFA] trying FF-A framework discovery
   [FFA] Conduit is SMC
   [FFA] FF-A driver 1.0
   FF-A framework 1.0
   [FFA] Versions are compatible
   Core:  18 devices, 12 uclasses, devicetree: separate
   MMC:
   Loading Environment from nowhere... OK
   ...
   Hit any key to stop autoboot:  0
   Loading kernel from 0x083EE000 to memory ...
   ...
   [FFA] endpoint ID is 0
   [FFA] Using 1 4KB page(s) for RX/TX buffers size
   [FFA] RX buffer at virtual address 00000000fdf4e000
   [FFA] TX buffer at virtual address 00000000fdf50000
   [FFA] RX/TX buffers mapped
   [FFA] Reading partitions data from the RX buffer
   [FFA] Partition ID 8001 : info cached
   [FFA] Partition ID 8002 : info cached
   [FFA] Partition ID 8003 : info cached
   [FFA] 3 partition(s) found and cached
   [FFA] Preparing for checking partitions count
   [FFA] Searching partitions using the provided UUID
   [FFA] No partition found. Querying framework ...
   [FFA] Reading partitions data from the RX buffer
   [FFA] Number of partition(s) matching the UUID: 1
   EFI: Pre-allocating 1 partition(s) info structures
   [FFA] Preparing for filling partitions info
   [FFA] Searching partitions using the provided UUID
   [FFA] Partition ID 8003 matches the provided UUID
   EFI: MM partition ID 0x8003
   EFI: Corstone1000: Capsule shared buffer at 0x80000000 , size 8192 pages
   Booting /MemoryMapped(0x0,0x88200000,0xf00000)
   EFI stub: Booting Linux Kernel...
   EFI stub: Using DTB from configuration table
   EFI stub: Exiting boot services...
   [FFA] removing the device
   [FFA] unmapping RX/TX buffers
   [FFA] Freeing RX/TX buffers
   Booting Linux on physical CPU 0x0000000000 [0x411fd040]
   Linux version 6.1.9-yocto-standard (oe-user@oe-host) (aarch64-poky-linux-musl-gcc (GCC) 12.2.0, GNU ld (GNU Binutils) 2.40.202301193
   Machine model: ARM Corstone1000 FPGA MPS3 board
   efi: EFI v2.100 by Das U-Boot
   ...

Contributors
------------
   * Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
