.. SPDX-License-Identifier: GPL-2.0+

.. sectionauthor:: Simon Glass <sjg@chromium.org>

.. toctree::
   :maxdepth: 6

.. contents:: Page Contents
   :depth: 2
   :local:
   :backlinks: none

==============
ACPI subsystem
==============

This section aims to explain a bit about ACPI and how to enable it for a
new board in U-Boot. For any questions, you can ask on the U-Boot
mailing list or irc.

The Role of ACPI: From Power Management to Hardware Discovery
=============================================================

ACPI was developed to replace a collection of older, disparate
standards, including Advanced Power Management (APM), the MultiProcessor
Specification, and the Plug and Play BIOS (PnP) Specification. Its
primary innovation was to shift the responsibility for power management
and hardware configuration from platform-specific firmware into the
control of a standardized OS component. This model is known as Operating
System-directed configuration and Power Management (OSPM).

Under this model, the firmware's role is not to perform power management
itself, but to provide the OS with a comprehensive, abstract description
of the platform's hardware components and capabilities. This description
is delivered through a series of data structures called ACPI tables,
which are placed in system memory by the bootloader before the OS takes
control. The OS then parses these tables to discover devices, configure
them, and manage their power states. This approach enables a single,
generic OS image to run on a wide variety of hardware platforms, a key
objective of initiatives like the Arm SystemReady certification program.

Comparison with device tree
---------------------------

If you are familiar with devicetree, the fundamental difference between
that and ACPI lies in their descriptive power. A devicetree Blob (DTB)
primarily describes *what* hardware exists—for example, specifying that
a particular model of UART controller is present at a given memory
address. It is incumbent upon the OS driver to possess the intrinsic
knowledge of *how* to operate that specific hardware. ACPI extends this
by providing not only static data tables but also a mechanism for
firmware to supply platform-independent, executable bytecode. This code,
written in ACPI Machine Language (AML), is stored within the ACPI tables
and executed by an interpreter within the OS kernel.

These AML "methods" can define platform-specific procedures for tasks
like changing power states, reading thermal sensors, or managing battery
charging logic. This provides a more powerful abstraction layer,
allowing hardware vendors to implement complex, proprietary sequences
behind a standard software interface, thereby reducing the need for
OS-specific driver modifications for minor hardware revisions.

Which is better?
----------------

It depends.

Proponents of ACPI assume that it is better. It is potentially easier to
get changes into the ACPI spec than go through Linux's process for
devicetree-schema review. ACPI has nice tables of fields in a document,
while devicetree's schema is spread over thousands of YAML files. Also
ACPI supports firmware-provided (but arch-neutral) code to perform
various operations, while describing such things in devicetree can be
painful. ACPI promises a true split between firmware and OS, just like
the old days, whereas with devicetree it is never clear whether a
firmware-provided devicetree will allow an OS to fully support the
hardware.

Devicetree proponents counter that at least you can read a devicetree
with a simple parser instead of having to understand dozens of custom
formats. Also, only the OS can understand the complexity of the system
and the relationships between its components. Hiding some of the pieces
within firmware makes it impossible for the OS to do its job, e.g. to
trade off CPU / GPU clock speeds against the skin temperature of a
laptop depending on the current workload. There is also the idea that it
is the OS' job to understand hardware (see
https://www.usenix.org/conference/osdi21/presentation/fri-keynote)

ACPI summary
------------

**Benefits:** 👍

-  **x86 compatibility:** Many of ACPI's features were designed with x86
      in mind and ACPI supports this architecture very well. Almost no
      x86 platforms use devicetree.

-  **Rich Power Management:** As its name suggests, ACPI excels at
      sophisticated power management. It enables fine-grained control
      over the power states of individual components (C-states) and the
      system as a whole (S-states), leading to features like sleep and
      hibernation.

-  **Standardized Interface:** ACPI provides a standardized interface
      for the OS to interact with the platform's firmware (UEFI/BIOS),
      promoting interoperability between different hardware vendors and
      operating systems.

-  **Vendor-Specific Customization:** ACPI allows manufacturers to embed
      ACPI Machine Language (AML) code in the firmware. This bytecode
      can implement platform-specific power management methods and
      hardware controls, offering flexibility for unique hardware
      designs.

**Disadvantages:** 👎

-  **Complexity:** The ACPI specification is huge and complex, which can
      make implementation and debugging a significant challenge for both
      firmware and OS developers.

-  **Proprietary "Blobs":** The ability to include executable AML
      bytecode can lead to "black box" scenarios where the OS has
      limited visibility into how certain hardware functions are
      controlled. This is a concern for security and for open-source
      purists.

-  **Potential for Bugs:** Flawed or poorly written ACPI firmware from
      vendors is a common source of system instability, incorrect
      hardware behavior, and power management issues in operating
      systems.

-  **Overkill for Simpler Systems:** For less complex, deeply embedded
      systems, the full feature set of ACPI is often unnecessary and
      adds needless complexity and overhead.

Devicetree summary
------------------

Common in the ARM and embedded systems landscape (like Android and
Raspberry Pi), the devicetree offers a more static and descriptive
approach to hardware enumeration.

**Benefits:** 👍

-  **Simplicity and Readability:** devicetree source files (.dts) are
      human-readable text files that describe the hardware in a
      hierarchical tree structure. This makes them much easier to
      understand and debug compared to ACPI's binary tables.

-  **Separation of Concerns:** The devicetree cleanly separates the
      hardware description from the kernel's driver code. This allows a
      single, generic kernel image to support multiple hardware
      platforms simply by loading a different devicetree blob (.dtb) at
      boot time.

-  **Static and Predictable:** The static nature of the devicetree
      provides a predictable and stable description of the hardware,
      which is ideal for embedded systems where the hardware
      configuration is fixed.

-  **No Executable Code:** The devicetree is purely a data structure. It
      describes *what* is there, but not *how* to control it. This
      avoids the potential for opaque, executable code in the hardware
      description, which is seen as more secure and transparent.

**Disadvantages:** 👎

-  **Static Nature:** The primary drawback of the devicetree is its
      static design. It's not well-suited for discovering hardware that
      can be added or removed while the system is running
      (hot-plugging).

-  **Limited Power Management Abstraction:** While a devicetree can
      describe power-related hardware, it lacks the sophisticated,
      standardized power management methods and policies found in ACPI.

-  **Verbosity for Complex Systems:** For highly complex systems with a
      large number of components, the devicetree source can become very
      verbose and cumbersome to manage.

-  **Kernel Dependency on "Bindings":** The kernel drivers must
      understand the specific properties and their meanings (the
      "bindings") used in the devicetree. A lack of standardized
      bindings across the ecosystem can lead to fragmentation.

Comparison Table: ACPI vs. devicetree
-------------------------------------

+----------------------+----------------------+----------------------+
| **Feature**          | **Advanced           | **Devicetree**       |
|                      | Configuration &      |                      |
|                      | Power Interface      |                      |
|                      | (ACPI)**             |                      |
+----------------------+----------------------+----------------------+
| **Primary            | **x86** (Desktops,   | **ARM/Embedded**     |
| Architecture**       | Laptops, Servers)    | (SBCs, IoT, Mobile)  |
+----------------------+----------------------+----------------------+
| **Core Philosophy**  | **Procedural**:      | **Descriptive**:     |
|                      | Tells OS *how* to do | Tells OS *what*      |
|                      | things               | hardware exists      |
+----------------------+----------------------+----------------------+
| **Hardware           | **Dynamic** at       | **Static**, defined  |
| Discovery**          | runtime, excellent   | at boot time         |
|                      | for hot-plug         |                      |
+----------------------+----------------------+----------------------+
| **Executable Code**  | **Yes**, runs ACPI   | **No**, it's purely  |
|                      | Machine Language     | a data structure     |
|                      | (AML) "blobs"        |                      |
+----------------------+----------------------+----------------------+
| **Power Management** | **Sophisticated &    | **Basic**, describes |
|                      | Standardized**       | hardware             |
|                      | (Sleep, Hibernate)   | capabilities only    |
+----------------------+----------------------+----------------------+
| **Complexity**       | **High**, vast       | **Low**,             |
|                      | specification, hard  | human-readable text  |
|                      | to debug             | files                |
+----------------------+----------------------+----------------------+
| **Best For**         | Systems with varied  | Systems with fixed,  |
|                      | and dynamic hardware | known hardware       |
+----------------------+----------------------+----------------------+

Getting started
===============

Table discovery
---------------

The OS's discovery of the ACPI environment begins with the Root System
Description Pointer (RSDP). The firmware places this structure in a
well-known memory location where the OS can find it, or point to it from
the EFI table list, when booting with EFI. The RSDP contains a pointer
to the main directory of all other ACPI tables, known as the Root System
Description Table (RSDT) or, for systems supporting 64-bit physical
addresses (ACPI 2.0 and later), the Extended System Description Table
(XSDT).

The RSDT/XSDT acts as a master index, containing the physical
memory-addresses of all other system description tables. Any errors in
the master index or any other tables, such as an incorrect pointer, a
bad checksum, or a malformed header, will typically cause the OS's ACPI
parser to fail, preventing the system from booting correctly or
discovering its devices.

Since the XSDT contains the final memory addresses of all other tables,
U-Boot uses a multi-stage generation process. The individual tables
(FADT, DSDT, etc.) are first generated and their sizes determined. After
that they are placed into a contiguous memory block, their final
addresses recorded, and the XSDT and RSDP constructed to point to them.

Common tables
-------------

Among the numerous tables defined by the ACPI specification, a few are
fundamental to describing a new board:

FADT (Fixed ACPI Description Table)
    This table provides
    pointers to fixed-function hardware registers essential for
    basic power management, such as the power management timer and
    control blocks. It also contains the physical address
    of the **DSDT** and a set of flags that describe the platform's
    architecture. For modern embedded systems, the
    HARDWARE_REDUCED_ACPI flag is particularly important, signaling
    to the OS that legacy PC hardware (like an i8259 PIC or CMOS
    RTC) is not present, which alters the OS's behavior accordingly.

DSDT (Differentiated System Description Table)
    This is the primary table for hardware description. It contains a single,
    large block of AML code that defines a hierarchical namespace of
    devices on the mainboard. This namespace, starting from a root
    scope called \\_SB (System Bus), describes every non-discoverable
    device (e.g., I2C sensors, serial ports, GPIO controllers), along
    with their resources and control methods. A system must have
    exactly one DSDT.

SSDT (Secondary System Description Table)
    These are optional tables that contain additional AML code. They are used to
    supplement the DSDT, often for modularity. For example, an SoC
    vendor might provide an SSDT describing the devices internal to
    the chip, while the board vendor provides the DSDT describing the
    devices on the PCB. A system can have multiple SSDTs

MADT (Multiple APIC Description Table)
    This is essential for multi-processor systems. It describes the interrupt
    controller topology to the OS. On x86 systems, it lists APIC and I/O APIC
    controllers. On ARM systems, it is used to describe the components
    of the Generic Interrupt Controller (GIC), such as the GIC
    Distributor (GICD), GIC CPU Interfaces (GICC), and GIC
    Redistributors (GICR).

+-----------+---------------------------+---------------------------+
| Signature | Table Name                | Purpose in U-Boot Context |
+-----------+---------------------------+---------------------------+
| RSD PTR   | Root System Description   | The entry point for the   |
|           | Pointer (RSDP)            | OS. U-Boot generates this |
|           |                           | last, pointing it to the  |
|           |                           | XSDT.                     |
+-----------+---------------------------+---------------------------+
| XSDT      | Extended System           | The master index of all   |
|           | Description Table         | other ACPI tables. U-Boot |
|           |                           | populates it with the     |
|           |                           | 64-bit physical addresses |
|           |                           | of all generated tables.  |
+-----------+---------------------------+---------------------------+
| FACP      | Fixed ACPI Description    | Describes fixed hardware  |
|           | Table (FADT)              | features and points to    |
|           |                           | the DSDT. U-Boot          |
|           |                           | populates it with         |
|           |                           | platform details and sets |
|           |                           | flags like                |
|           |                           | HARDWARE_REDUCED_ACPI.    |
+-----------+---------------------------+---------------------------+
| DSDT      | Differentiated System     | Contains the primary AML  |
|           | Description Table         | definition block          |
|           |                           | describing motherboard    |
|           |                           | devices. U-Boot           |
|           |                           | constructs this table     |
|           |                           | from fragments            |
|           |                           | contributed by various    |
|           |                           | device drivers.           |
+-----------+---------------------------+---------------------------+
| SSDT      | Secondary System          | Optional tables with      |
|           | Description Table         | additional AML            |
|           |                           | definitions. Can be used  |
|           |                           | to modularize device      |
|           |                           | descriptions.             |
+-----------+---------------------------+---------------------------+
| APIC      | Multiple APIC Description | Describes the system's    |
|           | Table (MADT)              | interrupt controller      |
|           |                           | topology (e.g., GIC on    |
|           |                           | ARM platforms).           |
+-----------+---------------------------+---------------------------+
| DBG2      | Debug Port Table 2        | Describes a hardware      |
|           |                           | debug port (e.g., a       |
|           |                           | serial port) for use by   |
|           |                           | the OS kernel debugger.   |
+-----------+---------------------------+---------------------------+
| IORT      | I/O Remapping Table       | Describes the I/O         |
|           |                           | topology, including       |
|           |                           | SMMUs, to the OS, which   |
|           |                           | is critical for DMA       |
|           |                           | operations on ARM64       |
|           |                           | systems.                  |
+-----------+---------------------------+---------------------------+

Enabling ACPI Support
---------------------

ACPI functionality is controlled at compile time through U-Boot's
Kconfig system. To enable ACPI for a new board, the following core
options must be set in the board's defconfig file:

+----------------------------+----------------------------------------+
| CONFIG_SUPPORT_ACPI        | Indicates whether the architecture     |
|                            | supports ACPI at all. This is enabled  |
|                            | for x86 and ARM.                       |
+----------------------------+----------------------------------------+
| CONFIG_ACPI                | Master switch to enable the core ACPI  |
|                            | subsystem. This is a prerequisite for  |
|                            | all other ACPI-related options.        |
+----------------------------+----------------------------------------+
| CONFIG_GENERATE_ACPI_TABLE | Enables creation of ACPI tables (as    |
|                            | opposed to receiving them from a prior |
|                            | boot stage). This is necessary if      |
|                            | U-Boot is the source of the ACPI       |
|                            | tables.                                |
+----------------------------+----------------------------------------+
| CONFIG_CMD_ACPI            | Includes the 'acpi' command, which is  |
|                            | an indispensable tool for on-target    |
|                            | inspection, verification, and          |
|                            | debugging of the generated tables.     |
+----------------------------+----------------------------------------+
| CONFIG_ACPIGEN             | Enables creation of ACPI tables from   |
|                            | code, rather than just relying on .asl |
|                            | files.                                 |
+----------------------------+----------------------------------------+

While ACPI is the default hardware description method for x86 platforms,
ARM and boards typically default to using devicetree. For these
architectures, it is often more convenient to use a configuration
fragment to enable all necessary ACPI options. U-Boot provides
acpi.config for this purpose, which can be appended to the make command
when creating the initial configuration::

    make qemu-arm64_defconfig acpi.config
    make

Just like devicetree, ACPIs are integrated with U-Boot's UEFI support.
When an OS is launched via the bootefi command, U-Boot provides a
pointer to the RSDP structure via the UEFI System Table, allowing the OS
loader (e.g., GRUB, Windows Boot Manager) to find the hardware
description.

Source-code Organization
------------------------

The main directories containing the ACPI implementation are:

include/acpi
    Public header files for the ACPI subsystem including
    acpigen.h, which defines the low-level API for generating AML
    bytecode, as well as
    acpi_table.h which defines the C structures for the standard
    ACPI tables.

lib/acpi
    Core, arch-independent logic for ACPI-table generation
    acpi_writer.c manages the list of registered table writers, and
    dsdt.c handles the assembly of the DSDT from various fragments.

arch/<arch>/lib
    Architecture-specific ACPI code and table-generation

drivers
    Individual device drivers throughout the source tree are
    responsible for contributing their own ACPI descriptions. This is
    where most of the board-specific ACPI code is added.

Code and .asl files
-------------------

U-Boot can provide ACPI tables to a target operating system through
several mechanisms. This document focuses on the two primary methods
where U-Boot is the source of the tables:

#. Dynamic Generation
      At boot time, U-Boot can generate ACPI tables
      programmatically. This is the preferred method for describing
      on-chip peripherals. Drivers within U-Boot's Driver Model (DM) can
      contribute AML "fragments" using a C-based API known as acpigen.
      These fragments are then assembled into the final Differentiated
      System Description Table (DSDT) or provided as Secondary System
      Description Tables (SSDTs).

#. Static Integration
      For platform-level definitions or complex
      hardware descriptions that are not tied to a specific driver,
      U-Boot can compile board-specific ACPI Source Language (.asl)
      files directly into the bootloader binary at build time. The build
      system invokes the Intel iasl compiler to convert these
      human-readable .asl files into binary AML, which is then embedded
      in the U-Boot image.

A third method, not covered in detail here, is the pass-through of ACPI
tables provided by a preceding boot stage, such as TIanocore. In this
scenario, U-Boot acts as a conduit, simply locating the existing tables
and passing them to the OS.

Integrating .asl files
----------------------

To integrate .asl files, place them in the board directory, typically
board/<vendor>/<board> or a subdirectory. Add rules to the Makefile for
each file. For example if your file is `my_board_acpi.asl`, then::

    # In board/<vendor>/<board>/Makefile
    obj-y += my_board_acpi.o

Ensure that the Intel ACPI compiler (iasl) is installed. On most Linux
distributions, this is provided by the acpica-tools package.

Preprocessing
~~~~~~~~~~~~~

U-Boot runs the C preprocessor (cpp) on all asl files mentioned in the
Makefile, allow access to Kconfig options. This helps to bridge the gap
between asl files and the rest of the build. You can use this to control
which ACPI features are available, or to access hard-coded values shared
with the rest of your board code, for example.

Code Example: A Simple .asl File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following is a minimal example of an .asl file that could be placed
at board/my_vendor/my_board/platform.asl and integrated by adding
`'obj-y += platform.o` to the corresponding Makefile::

    /*
     * platform.asl - Static ACPI definitions for My Board
     */
    DefinitionBlock ("DSDT.aml", "DSDT", 2, "U-BOOT", "MYBOARD", 1)
    {
    /*
     * Use a Kconfig option to conditionally include a device.
     * This block will only be included if CONFIG_MY_FEATURE=y
     * is set in the board's defconfig.
     */
    #ifdef CONFIG_MY_FEATURE
    Scope (\_SB)
    {
        Device (FEAT)
        {
            Name (\_HID, "MYVN0001") // A vendor-defined Hardware ID
            Name (\_UID, 0)
            Name (\_STA, 0x0F) // Status: Present, Enabled, Shown, Functioning
        }
    }
    #endif
    }

ACPI-Generation Flow
~~~~~~~~~~~~~~~~~~~~

The ACPI tables are constructed dynamically during the board_init_r()
stage of U-Boot's boot process, after main memory has been initialized
and the driver model is active. This ensures they are available to
inspect at the U-Boot command prompt.

The generation process is modular, consisting of two parts:

#. Table Writers
      These are C functions responsible for generating an entire
      ACPI table. U-Boot maintains a list of registered writer functions
      for standard tables like the FADT, MADT, and IORT. These writers
      are typically generic and gather system-wide information to
      construct their respective tables.

#. Device Fragments
      A device can contribute a small fragments of
      AML code that describe itself. These fragments are
      almost always destined for inclusion in the DSDT or an SSDT. As
      the U-Boot Driver Model probes and initializes devices, any driver
      that supports ACPI can generate its AML description and register
      it with the core ACPI subsystem.

This modular design means that the core ACPI code does not need to know
about every device in the system. Instead, it acts as an assembler.
After all drivers have been probed, a finalization function,
acpi_write_tables(), is called, which goes through these steps:

#. Collect all the registered DSDT/SSDT fragments from the probed
   devices.

#. Call all registered table writer functions to generate the whole
   tables (FADT, MADT, etc.).

#. Calculate the final size and memory offset for each table and
   fragment.

#. Lay out all the binary data contiguously in a reserved region of
   DRAM, typically in a bloblist entry

#. Construct the XSDT (or RSDT), populating it with the final 64-bit
   (32-bit) physical addresses of each table.

#. Create the RSDP structure, pointing it to the newly created XSDT,
   storing its address for the 'acpi' and boot commands to use.

This deferred assembly is the key architectural pattern of U-Boot's ACPI
implementation. Functions that drivers call to add ACPI information do
not write directly to the final memory location; they add a structured
"item" to a central list, which is processed in a final, atomic step.

acpigen API: Building AML Bytecode in C
---------------------------------------

The core of U-Boot's ACPI generation capability is the acpigen API,
defined in include/acpi/acpigen.h. This API, inspired by coreboot,
provides a set of C functions that act as a low-level assembler for
creating AML bytecode, abstracting away the raw opcodes and package
length encoding rules defined in the ACPI specification.

struct acpi_ctx
~~~~~~~~~~~~~~~

Nearly every function in the acpigen API takes a pointer to a struct
acpi_ctx as its first parameter. This structure serves as the state
machine for the entire generation process for a given table or fragment.
It is initialized by the core ACPI framework and tracks the current
write position in the memory buffer allocated for the AML code.

The context is used to handle nested, variable-length AML structures.
Many AML constructs, such as Device(), Package(), and
ResourceTemplate(), require their total length to be encoded in their
header. Since the content is generated dynamically, this length is not
known in advance. The acpi_ctx structure solves this by maintaining an
internal stack where it can store the memory addresses of these length
placeholders. When a variable-length block is closed, the API can
retrieve the placeholder's address from the stack, calculate the length
of the content that was just written, and patch the correct value into
the header.

Core Functions
~~~~~~~~~~~~~~

These functions support generation of common AML bytecodes.:

Namespace and Scoping
   **acpigen_write_scope(ctx, name)**: Generate a Scope() operator,
   which changes the current position in the ACPI namespace. All
   subsequent objects are created within this scope.

   **acpigen_write_device(ctx, name)**: Generate a Device() operator,
   which defines a new device object in the current scope. This is
   a variable-length object that must be closed with
   acpigen_pop_len().

Named Objects
   **acpigen_write_name_string(ctx, name, value)**: Create a named
   object with a string value (e.g., Name(\_HID, "ARMH0011")).

   **acpigen_write_name_integer(ctx, name, value)**: Create a named
   object with an integer value (e.g., Name(\_UID, 0)).

   **acpigen_write_name_byte(ctx, name, value)**: Create a named object
   with a single byte integer value.

Raw Data Types
   **acpigen_write_integer(ctx, value)**: Write an ACPI integer object
   (which can be a byte, word, dword, or qword depending on the
   value).

   **acpigen_write_byte(ctx, value)**: Write a raw byte to the stream.

Low-Level Streaming
   **acpigen_emit_stream(ctx, stream, size)**: For complex or static AML
   sequences, this function provides a way to write a pre-defined
   array of bytes directly into the context buffer. This is often
   used for boilerplate methods like \_STA or empty resource
   templates.

Variable-Length Packages
~~~~~~~~~~~~~~~~~~~~~~~~

To create variable-length structures, your code must follow these steps:

#. Start the Block
    To begin any variable-length structure (like a
    Device block, a Package, or a ResourceTemplate), call a function
    that writes the appropriate header and a placeholder for the
    length, e.g. acpigen_write_len_f(ctx). This pushes the address of
    the length value onto the internal stack within the acpi_ctx.

#. Write the Content
    Call other acpigen functions to generate the
    content of the block (e.g., \_HID, \_CRS objects inside a Device
    block).

#. Close the Block
    Call acpigen_pop_len(ctx), which retrieves
    (pops) the placeholder address from the acpi_ctx stack, calculates
    the size of the content written since the corresponding
    acpigen_write_len_f() call, and patches the correct encoded length
    into the placeholder's location in memory.

This `acpigen_write_len_f()` / `acpigen_pop_len()` pairing can be nested, as
the acpi_ctx stack can handle multiple pending length calculations. For example,
creating a Package() inside a Device() block would follow this sequence::

    // Start Device() block
    acpigen_write_device(ctx, "DEV0");

    //... write some objects for DEV0...
    // Start an inner Package()
    acpigen_write_package(ctx, 2); // Package with 2 elements
    acpigen_write_integer(ctx, 123);
    acpigen_write_string(ctx, "ABC");
    acpigen_pop_len(ctx); // Close Package()

    //... write more objects for DEV0...

    // Close Device() block
    acpigen_pop_len(ctx);

Driver Model ACPI Hook
~~~~~~~~~~~~~~~~~~~~~~

Driver model provides support for generating ACPI tables from a device
driver, using the a struct acpi_ops pointer within the driver
declaration. The code is automatically omitted when ACPI is not in use.

There are four main methods:

get_name()
    Obtain the ACPI name of a device

write_tables()
    Write out any additional (whole) tables required by this device

fill_ssdt()
    Generate SSDT code for a device

inject_dsdt()
    Generate DSDT code for a device

Notes that the SSDT table can only be created using C code, while the
DSDT table can have a base defined by '.aml', with additional code
'injected' into it by the last method above.

Since U-Boot always uses devicetree internally, your device must be
present in that devices for it to be bound and for ACPI generation to
occur. Use 'dm tree' to check what devices are bound.

Describing Device Resources (\_HID, \_UID, \_CRS)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To be useful to the OS, a device's AML description must include a
standard set of objects that define its identity, resources, and status.
The four most fundamental objects are:

\\_HID (Hardware ID)
    A string that uniquely identifies the
    device's type. The OS uses this ID to find and load the
    appropriate driver. These IDs are standardized; for instance, an
    ARM PL011-compatible UART uses the \_HID "ARMH0011".

\\_UID (Unique ID)
    An integer or string used to distinguish
    between multiple instances of the same device type. For a device
    that is the only one of its kind on the bus, this is typically set
    to 0 or 1.

\\_CRS (Current Resource Settings)
    This is a method that, when
    called by the OS, returns a ResourceTemplate package. This package
    contains a list of descriptors for all the hardware resources the
    device requires, such as memory-mapped register blocks,
    interrupts, GPIOs, and I2C or SPI connections. This is the most
    critical object for enabling the OS driver to communicate with the
    hardware.

\\_STA (Status)
    A method that returns an integer indicating the
    device's current status. A return value of 0xF (or 0b1111)
    indicates that the device is present, enabled, visible in the
    UI, and functioning correctly. For most statically configured
    devices in U-Boot, this method can simply be implemented to
    always return
    0xF.

Simple example: Describing a UART Device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following C code snippet demonstrates how a driver for a
PL011-compatible UART might implement an ACPI generation method. This
code should reside within the driver file. It is called by the ACPI
core:

.. code-block:: c

    #include <dm.h>
    #include <acpi/acpigen.h>
    #include <acpi/acpi_device.h>

    /* Called by the ACPI core to generate the AML description for a UART device */
    int uart_gen_acpi(const struct udevice *dev, struct acpi_ctx *ctx)
    {
        /* Get device-specific data, like register base address */
        struct uart_priv *priv = dev_get_priv(dev);
        char scope[ACPI_PATH_MAX];
        char name[ACPI_NAME_MAX];
        pci_dev_t bdf;
        u32 address;
        int ret;

        ret = acpi_device_scope(dev, scope, sizeof(scope));
        if (ret)
            return log_msg_ret("scope", ret);
        ret = acpi_get_name(dev, name);
        if (ret)
            return log_msg_ret("name", ret);

        u64 reg_base = priv->reg_base;
        u32 irq = priv->irq;

        /* Start the Device definition with its name, e.g., "UAR0" */
        acpigen_write_scope(ctx, scope);
        acpigen_write_device(ctx, name);

        /* Define the Hardware ID (_HID) and Unique ID (_UID) */
        acpigen_write_name_string(ctx, "_HID", "ARMH0011"); /* Standard HID for PL011 */
        acpigen_write_name_integer(ctx, "_UID", 0);
        acpigen_write_sta(ctx, acpi_device_status(dev));

        /* Address */
        bdf = dm_pci_get_bdf(dev);
        address = (PCI_DEV(bdf) << 16) | PCI_FUNC(bdf);
        acpigen_write_name_dword(ctx, "_ADR", address);

        acpigen_pop_len(ctx); /* Device */
        acpigen_pop_len(ctx); /* Scope */

        return 0;
    }


Full examples in the code
~~~~~~~~~~~~~~~~~~~~~~~~~

There are many examples you can follow in the code, such as:

Coral (Intel Apollo Lake)
    Has an `inject_dsdt()` function called `chromeos_acpi_gpio_generate()`
    which adds GPIOs into the DSDT.

    Coral also has a write_tables() function called
    coral_write_acpi_tables() which adds an NHLT (Native Intel
    Low-Power Translation) table

Designware I2C driver (for PCI)
    Includes a fill_ssdt() function
    called dw_i2c_acpi_fill_ssdt() which sets the input-clock
    frequency and the settings for each I2C speed

Raspberry Pi
    Has a fill_ssdt() function which calls
    armv8_cpu_fill_ssdt() to set up information about the available
    CPUs, as well as a fill_madt() for creating an MADT.

Example patch series
    The
    `2024 patch series <https://patchwork.ozlabs.org/project/uboot/list/?series=429385&state=*>`_
    for Raspberry Pi shows adding ACPI support for an
    ARM board. Note that there are several patches which are adding more ARM
    support, which can be ignored.

Intel's Minnowboard Max
    This is an older and simpler x86 board with some
    ACPI tables (in board/intel/minnowmax/dsdt.asl and within that
    acpi/mainboard.asl). There are quite a lot of other boards in
    board/intel so it can be a useful reference.

Verification and Debugging
==========================

As mentioned, if the ACPI tables are wrong you are going to have
problems. U-Boot provides some commands to help with this and external
tools can be used for deeper analysis.

Using the acpi Command
----------------------

The :doc:`/usage/cmd/acpi` is the primary tool for inspecting the generated
tables in memory. It offers several sub-commands for different levels of
detail.

acpi list
    Display a summary of all the top-level
    ACPI tables that have been generated and placed in memory. It
    shows each table's signature (e.g., XSDT, FACP, DSDT), its base
    address, size, and other header information. Adding the -c flag
    (acpi list -c) performs a checksum verification on each table,
    which is an essential first step to catch basic corruption or
    generation errors::

        => acpi list -c
        Name Base Size Detail
        ---- -------- ----- ------
        RSDP 79925000 24 v02 U-BOOT OK
        XSDT 799250e0 6c v01 U-BOOT U-BOOTBL 20220101 INTL 0 OK
        FACP 79929570 f4 v04 U-BOOT U-BOOTBL 20220101 INTL 1 OK
        DSDT 79925280 32ea v02 U-BOOT U-BOOTBL 20110725 INTL 20180105 OK

acpi items
    Provide a more fined grain view,
    listing every individual component that was assembled into the
    final tables. It distinguishes between other items (whole tables
    generated by a writer function) and dsdt or ssdt items (fragments
    contributed by a specific device driver). This is extremely useful
    for confirming that a particular driver's ACPI hook was
    successfully executed

    The output below shows that the device wacom-digitizer@9 contributed a
    273-byte (0x111 fragment to the SSDT. If this entry were missing, it
    might indicate that the driver is not bound (check `dm tree`) or its
    ACPI methods are missing::

        => acpi items
        Seq  Type       Base   Size  Device/Writer
        ---  -----  --------   ----  -------------
          0  other  77b09000    240  0base
          1  other  77b09240     40  1facs
          2  dsdt   77b092a4     58  board
          3  dsdt   77b092fc     10  lpc
          4  other  77b092fc   3274  3dsdt
          5  other  77b0c570   1000  4gnvs
          6  other  77b0d570     30  5csrt
          7  other  77b0d5a0    120  5fadt
          8  other  77b0d6c0     70  5madt
          9  other  77b0d730     40  5mcfg
          a  other  77b0d770     50  5spcr
          b  other  77b0d7c0     50  5tpm2
          c  ssdt   77b0d834     fe  maxim-codec
          d  ssdt   77b0d932     28  i2c2@16,0
          e  ssdt   77b0d95a    270  da-codec
          f  ssdt   77b0dbca     28  i2c2@16,1
         10  ssdt   77b0dbf2     28  i2c2@16,2
         11  ssdt   77b0dc1a     83  tpm@50
         12  ssdt   77b0dc9d     28  i2c2@16,3
         13  ssdt   77b0dcc5    282  elan-touchscreen@10
         14  ssdt   77b0df47    285  raydium-touchscreen@39
         15  ssdt   77b0e1cc     28  i2c2@17,0
         16  ssdt   77b0e1f4     d8  elan-touchpad@15
         17  ssdt   77b0e2cc    163  synaptics-touchpad@2c
         18  ssdt   77b0e42f     28  i2c2@17,1
         19  ssdt   77b0e457    111  wacom-digitizer@9
         1a  ssdt   77b0e568     8f  sdmmc@1b,0
         1b  ssdt   77b0e5f7     4b  wifi
         1c  ssdt   77b0e642    1a0  cpu@0
         1d  ssdt   77b0e7e2    1a0  cpu@1
         1e  ssdt   77b0e982    1a0  cpu@2
         1f  ssdt   77b0eb22    211  cpu@3
         20  other  77b0eb22    21e  6ssdt
         21  other  77b11ba0     b0  8dev
        =>

acpi dump <NAME>
    Dump the full binary content of a single,
    assembled table, specified by its 4-character name (e.g. 'acpi
    dump DSDT'). This is useful for in-depth manual inspection against
    the ACPI specification, for those who can bear the pain of
    byte-by-byte comparison against a hierarchical format. For example::

        => acpi dump DBG2
        DBG2 @         77b11ba0
        00000000: 44 42 47 32 61 00 00 00 00 fa 55 2d 42 4f 4f 54  DBG2a.....U-BOOT
        00000010: 55 2d 42 4f 4f 54 42 4c 01 08 25 20 49 4e 54 4c  U-BOOTBL..% INTL
        00000020: 00 00 00 00 2c 00 00 00 01 00 00 00 00 35 00 01  ....,........5..
        00000030: 0f 00 26 00 00 00 00 00 00 80 00 00 00 00 16 00  ..&.............
        00000040: 22 00 00 00 00 03 00 00 00 de 00 00 00 00 00 10  "...............
        00000050: 00 00 5c 5f 53 42 2e 50 43 49 30 2e 55 52 54 33  ..\_SB.PCI0.URT3
        00000060: 00                                               .
        =>

acpi set <addr>
    Manually set the base address of the ACPI tables
    that the command will inspect. This is an advanced feature for
    debugging tables that have been loaded into memory from an
    external source (e.g., from a file via TFTP) instead of being
    generated by U-Boot.

Dump Binary Data
    For deep inspection, the `acpi items -d` command
    provides a binary hex dump of each item. You can see the AML
    bytecode that were generated by their acpigen calls. This binary
    data can be saved and analyzed with external using the 'save'
    command, for analysis with external tools.

This debugging workflow provides a powerful, step-by-step method for
verification:

#. If a device is not functioning correctly in the OS, check
   `acpi list -c` to make sure there are no table-level errors.

#. If the tables are valid, `acpi items` can confirm that the device contributed
   its AML fragment

#. If it did, `acpi items -d` can be used to extract the raw bytecode.
   This bytecode can then be disassembled using a standard tool like Intel's
   iasl compiler/disassembler (part of the ACPICA toolkit) to verify that the
   generated AML is syntactically correct and matches the intended logic.
   You may need to use the `save` command to write the data to a file
   on a USB stick, or the :doc:`/usage/cmd/tftpput` to write it to a network
   server.

This process allows for a complete
verification cycle, isolating problems to either the U-Boot generation
code or the OS driver's interpretation of the ACPI tables.

Booting the OS
--------------

If the OS can boot with your ACPI tables, then you have a few more
options:

Extract Tables
    Once the OS is running, extract the ACPI tables
    that it is actually using. This can be done with the `acpidump`
    utility or by directly reading the binary table files from the
    sysfs directory at `/sys/firmware/acpi/tables/`

Disassemble AML
    Use the `iasl` compiler to disassemble the
    extracted binary tables (.dat files) back into human-readable ACPI
    Source Language (.dsl files). For example::

        iasl -d *.dat

    will process all tables in the current directory, so you can see the
    AML code the OS kernel is interpreting.

Analyze with fwts
    The Firmware Test Suite (fwts) is a powerful
    tool that can automatically analyze ACPI tables for common errors,
    specification violations, and inconsistencies. Running fwts on the
    extracted tables can quickly identify many subtle bugs that might
    be missed by manual inspection.

Common Pitfalls and Tips
------------------------

Operating System Caching
    Be aware that some operating systems,
    particularly Windows, aggressively cache ACPI tables to speed up
    boot times. If a developer modifies the ACPI tables in U-Boot and
    re-flashes the firmware, the OS may continue to use the old,
    cached version. To force the OS to re-read the tables, the
    revision number in the table's header must be incremented. This
    can be done in the DefinitionBlock of an .asl file or in the C
    code that generates the table header.

Compiler Strictness
    The open-source iasl compiler is often more
    strict in its interpretation of the ACPI specification than the
    proprietary compilers used by some BIOS vendors. AML code that
    works on one system may generate warnings or errors when compiled
    with iasl. These warnings should be treated as potential bugs and
    investigated, as they may indicate latent problems that could
    affect OS stability.

Checksums
    A failed checksum is an immediate red flag. The OS
    will reject any table with an invalid checksum. The 'acpi list -c'
    command should always be the first step in any debugging session.

Send patches
============

Once you have your board working with ACPI, please send patches to the
mailing list. This can help others using similar hardware. If your
patches are incomplete, they can still be useful. You may even get some
help!
