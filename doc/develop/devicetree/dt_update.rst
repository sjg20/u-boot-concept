.. SPDX-License-Identifier: GPL-2.0+

Updating the devicetree
=======================

U-Boot uses devicetree for runtime configuration and storing required blobs or
any other information it needs to operate. This is called the 'control'
devicetree since it controls U-Boot. It is possible to update the control
devicetree separately from actually building U-Boot. This provides a good degree
of control and flexibility for firmware that uses U-Boot in conjunction with
other project.

There are many reasons why it is useful to modify the devicetree after building
it:

- Configuration can be changed, e.g. which UART to use
- A serial number can be added
- Public keys can be added to allow image verification
- Console output can be changed (e.g. to select serial or vidconsole)

This section describes how to work with devicetree to accomplish your goals.

See also :doc:`../devicetree/control` for a basic summary of the available
features.


Devicetree source
-----------------

Every board in U-Boot must include a devicetree sufficient to build and boot
that board on suitable hardware (or emulation). This is specified using the
`CONFIG DEFAULT_DEVICE_TREE` option.


Building the devicetree
-----------------------

U-Boot automatically builds the devicetree for a board, from the
`arch/<arch>/dts` directory. The Makefile in those directories has rules for
building devicetree files. It is preferable to avoid target-specific rules in
those files: i.e. all boards for a particular SoC should be built at once,
where practical. Apart from simplifying the Makefile, this helps to efficiently
(and immediately) ensure that changes in one board's DT do not break others that
are related. Building devicetrees is fast, so performance is seldom a concern
here.


Overriding the default devicetree
---------------------------------

When building U-Boot, the `DEVICE_TREE` environment variable allows the
default devicetree file to be overridden at build time. This can be useful if
modifications have to be made to the in-tree devicetree file, for the benefit
of a downstream build system. Note that the in-tree devicetree must be
sufficient to build and boot, so this is not a way to bypass that requirement.


Modifying the devicetree after building
---------------------------------------

While it is generally painful and hacky to modify the code or rodata of a
program after it is built, in many cases it is useful to do so, e.g. to add
configuration information like serial numbers, enabling/disabling features, etc.

Devicetree provides a very nice solution to these problems since it is
structured data and it is relatively easy to change it, even in binary form
(see fdtput).

U-Boot takes care that the devicetree is easily accessible after the build
process. In fact it is placed in a separate file called `u-boot.dtb`. If the
build system wants to modify or replace that file, it can do so. Then all that
is needed is to run `binman update` to update the file inside the image. If
binman is not used, then `u-boot-nodtb.bin` and the new `u-boot.dtb` can simply
be concatenated to achieve the desired result. U-Boot happily copes with the
devicetree growing or shrinking.

The `u-boot.bin` image contains both pieces. While it is possible to locate the
devicetree within the image using the signature at the start of the file, this
is a bit messy.

This is why `CONFIG_OF_SEPARATE` should always be used when building U-Boot.
The `CONFIG_OF_EMBED` option embeds the devicetree somewhere in the U-Boot ELF
image as rodata, meaning that it is hard to find it and it cannot increase in
size.

When modifying the devicetree, the different cases to consider are as follows:

- CONFIG_OF_SEPARATE
    This is easy, described above. Just change, replace or rebuild the
    devicetree so it suits your needs, then rerun binman or redo the `cat`
    operation to join `u-boot-nodtb.bin` and the new `u-boot.dtb`

- CONFIG_OF_EMBED
    This is tricky, since the devicetree cannot easily be located. If the EFL
    file is available, then the _dtb_dt_begin and __dtb_dt_end symbols can be
    examined to find it. While it is possible to contract the file, it is not
    possible to expand the file since that would involve re-linking

- CONFIG_OF_BOARD
    This is a board-specific situation, so needs to be considered on a
    case-by-case base.


Use of U-Boot /config node
--------------------------

A common problem with firmware is that many builds are needed to deal with the
slight variations between different, related models. For example, one model may
have a TPM and another may not. Devicetree provides an excellent solution to
this problem, in that the devicetree to actually use on a platform can be
injected in the factory based on which model is being manufactured at the time.

A related problem causing build proliferation is dealing with the differences
between development firmware, developer-friendly firmware (e.g. with all
security features present but with the ability to access the command line),
test firmware (which runs tests used in the factory), final production firmware
(before signing), signed firmware (where the signatures have been inserted) and
the like. Ideally all or most of these should use the same U-Boot build, with
just some options to determine the features available. For example, being able
to control whether the UART console or JTAG are available, on any image, is a
great debugging aid.

When the firmware consists of multiple parts, it is helpful that all operate
the same way at runtime, regardless of how they were built. This can be achieved
by passing the runtime configuration (e.g. 'enable UART console) along the chain
through each firmware stage. It is frustrating to have to replicate a bug on
production firmware which does happen on developer firmware, because they are
completely different builds.

The /config node provides useful functionality for this. It allows the different
controls to be 'factored out' of the U-Boot binary, so they can be controlled
separately from the initial source-code build. The node can be easily updated by
a build or factory tool and can control various features in U-Boot. It is
similar in concept to a Kconfig option, except that it can be changed after
U-Boot is built.

The /config node is similar in concept to the `/chosen node`_ except that it is
for passing information *into* firmware instead of from firmware to the
Operating System. Also, while Linux has a (sometimes extremely long) command
line, U-Boot does not support this. The devicetree provides a more structured
approach in any case. Upstreaming of this node (as /options) has begun as of
November 2021.


Devicetree in another project
-----------------------------

In some cases U-Boot receives its devicetree at runtime from a program that
calls it. For example ARM's Trusted Firmware A (`TF-A`_) may have a devicetree
that it passes to U-Boot. This overrides any devicetree build by U-Boot. When
packaging the firmware, the U-Boot devicetree may in fact be left out if it can
be guaranteed that it will receive one from another project.

In this case, the devicetree in the other project must track U-Boot's use of
device tree, for the following reasons:

- U-Boot only has one devicetree. See `Why not have two devicetrees?`_.
- For a consistent firmware build, decisions made in early stages should be
  communicated to later ones at runtime. For example, if the serial console is
  enabled in an early stage, it should be enabled in U-Boot too.
- U-Boot is quite capable of managing its own copy of the devicetree. If
  another project wants to bypass this (often for good reason), it is reasonable
  that it should take on the (fairly small) requirements that U-Boot features
  that rely on devicetree are still available
- The point here is not that *U-Boot needs this extra node*, or *U-Boot needs
  to have this public key*. These features are present in U-Boot in service of
  the entire firmware system. If the U-Boot features are used, but cannot be
  supported in the normal way, then there is pressure to implement these
  features in other ways. In the end, we would have a different mechanism for
  every other project that uses U-Boot. This introduces duplicate ways of doing
  the same thing, needlessly increases the complexity of the U-Boot source code,
  forces authors to consider parallel implementations when writing new features,
  makes U-Boot harder to test, complicates documentation and confuses the
  runtime flow of U-Boot. If every board did things its own way rather than
  contributing to the common code, U-Boot would lose a lot of its cross-platform
  value.

The above does not indicate *bad design* within U-Boot. Devicetree is a core
component of U-Boot and U-Boot makes use of it to the full. It solves a myriad
of problems that would otherwise need their own special C struct, binary format,
special property, tooling for viewing and updating, etc.

Specifically, the other project must provide a way to add configuration and
other information to the devicetree for use by U-Boot, such as the /config node.
Note that the U-Boot in-tree devicetree source must be sufficient to build and
boot, so this is not a way to bypass that requirement.

If binman is used, the devicetree source in U-Boot must contain the binman
definition so that a valid image can be build. This helps people discover what
other firmware components are needed and seek out appropriate documentation.

If verified boot is used, the project must provide a way to inject a public key,
certificate or other material into the U-Boot devicetree so that it is available
to U-Boot at runtime. See `Signing with U-Boot devicetree`_. This may be
through tooling in the project itself or by making use of U-Boot's tooling.


Devicetree generated on-the-fly in another project
--------------------------------------------------

In some rare cases, another project may wish to create a devicetree for U-Boot
entirely on-the-fly, then pass it to U-Boot at runtime. The only known example
of this at the time of writing (2021) is QEMU, for ARM (`QEMU ARM`_) and
RISC-V (`QEMU RISC-V`_).

In effect, when the board boots, U-Boot is *downstream* of the other project.
It is entirely reliant on that project for its correct operation.

This does not mean to imply that the other project is creating its own,
incompatible devicetree. In fact QEMU generates a valid devicetree which is
suitable for both U-Boot and Linux. It is quite normal for a devicetree to be
present in flash and be made available to U-Boot at runtime. What matters is
where the devicetree comes from. If the other project builds a devicetree for
U-Boot then it needs to support adding the things needed by U-Boot features.
Without them, for example:

- U-Boot may not boot because too many devices are enabled before relocation
- U-Boot may not have access to the developer or production public keys used for
  signing
- U-Boot may not support controlling whether the console is enabled
- U-Boot may not be know which MMC device to boot from
- U-Boot may not be able to find other firmware components that it needs to load

Normally, supporting U-Boot's features is trivial, since the devicetree compiler
(dtc) can compile the source, including any U-Boot pieces. So the burden is
extremely low.

In this case, the devicetree in the other project must track U-Boot's use of
device tree, so that it remains compatible. See `Devicetree in another project`_
for reasons why.

If a particular version of the project is needed for a particular version of
U-Boot, that must be documented in both projects.

Further, it must provide a way to add configuration and other information to
the devicetree for use by U-Boot, such as the `/config` node and the tags used
by driver model. Note that the U-Boot in-tree devicetree must be sufficient to
build and boot, so this is not a way to bypass that requirement.

More specifically, tooling or command-line arguments must provide a way to
add a `/config` node or items within that node, so that U-Boot can receive a
suitable configuration. It must provide a way of adding `u-boot,dm-...` tags for
correct operation of driver model. These options can then be used as part of the
build process, which puts the firmware image together. For binman, a way must be
provided to add the binman definition into the devicetree in the same way.

One way to do this is to allow a .dtsi file to be merged in with the generated
devicetree.

Note that the burden goes both ways. If a new feature is added to U-Boot which
needs support in another project, then the author of the U-Boot patch must add
any required support to the other project.


Passing the devicetree through to Linux
---------------------------------------

Ideally U-Boot and Linux use the same devicetree source, even though it is
hosted in separate projects. U-Boot adds some extra pieces, such as the
`config/` node and tags like `u-boot,dm-spl`. Linux adds some extra pieces, such
as `linux,default-trigger` and `linux,code`. This should not interfere with
each other.

In principle it is possible for U-Boot's control devicetree to be passed to
Linux. This is, after all, one of the goals of devicetree and the original
Open Firmware project, to have the firmware provide the hardware description to
the Operating System.

For boards where this approach is used, care must be taken. U-Boot typically
needs to 'fix up' the devicetree before passing it to Linux, e.g. to add
information about the memory map, about which serial console is used, provide
the kernel address space layout randomization (KASLR) seed or select whether the
console should be silenced for a faster boot.

Fix-ups involve modifying the devicetree. If the control devicetree is used,
that means the control devicetree could be modified, while U-Boot is using it.
Removing a device and reinserting it can cause problems if the devicetree offset
has changed, for example, since the device will be unable to locates its
devicetree properties at the expected devicetree offset, which is a fixed
integer.

To deal with this, it is recommended to employ one or more of the following
approaches:

- Make a copy of the devicetree and 'fix up' the copy, leaving the control
  devicetree alone
- Enable `CONFIG_OF_LIVE` so that U-Boot makes its own copy of the devicetree
  during relocation; fixups then happen on the original flat tree
- Ensure that fix-ups happen after all loading has happened and U-Boot has
  completed image verification

In practice,the last point is typically observed, since boot_prep_linux() is
called just before jumping to Linux, long after signature verification, for
example. But it is important to make sure that this line is not blurred,
particularly if untrusted user data is involved.


Devicetree use cases that must be supported
-------------------------------------------

Regardless of how the devicetree is provided to U-Boot at runtime, various
U-Boot features must be fully supported. This section describes some of these
features and the implications for other projects.

If U-Boot uses its own in-tree devicetree these features are supported
automatically.


Signing with U-Boot devicetree
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

U-Boot supports signing a payload so that it can be verified to have been
created by a party owning a private key. This is called verified boot in U-Boot
(see doc/uImage.FIT/verified-boot.txt).

Typically this works by creating a FIT and then running the `mkimage` tool to
add signatures for particular images. As part of this process, `mkimage` writes
a public key to the U-Boot devicetree, although this can be done separately.
See fdt_add_pubkey_ for patches for a suitable tool, for example.

As with all configuration information, if another project is providing the
devicetree to U-Boot, it must provide a way to add this public key into the
devicetree it passes to U-Boot. This could be via a tooling option, making use
of `mkimage`, or allowing a .dtsi file to be merged in with what is generated in
the other project.


Providing the binman image definition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In complex systems U-Boot must locate and make use of other firmware components,
such as images for the user interface, files containing peripheral firmware,
multiple copies of U-Boot for use with A/B boot, etc. U-Boot uses
:doc:`Binman <../package/binman>` as a standard way of putting an image
together.

Typically this works by running binman with the devicetree as an input, to
create the file image. Binman then outputs an updated devicetree which is
packed in the firmware image, so U-Boot can access the binman definition and
locate all the components.

As with all configuration information, if another project is providing the
devicetree to U-Boot, it must provide a way to add this binman definition into
the devicetree it passes to U-Boot. This could be via a tooling option, making
use of `binman`, or alowing a .dtsi file to be merged in with what is generated
in the other project.


Protecting the devicetree
-------------------------

U-Boot relies heavily on devicetree for correct operation. A corrupt or invalid
device can cause U-Boot to fail to start, behave incorrectly, crash (e.g. if
`CONFIG_OF_LIBFDT_ASSUME_MASK` is adjusted, or fail to boot an Operating System.
Within U-Boot, the devicetree is as important as any other part of the source
code. At ruuntime, the devicetree can be considered to be structured rodata.

With secure systems, care must be taken that the devicetree is valid:

- If the code / rodata has a hash or signature, the devicetree should also, if
  they are packaged separately.
- If the code / rodata is write-protected when running, the devicetree should be
  also. Note that U-Boot relocates its code and devicetree, so this is not as
  simple as it sounds. U-Boot must write-protect these items after relocating.


Why does U-Boot have its nodes and properties?
----------------------------------------------

See also :doc:`../devicetree/intro`.

There has been pushback at the concept that U-Boot dares have its own nodes and
properties in the devicetree.

Apart from these nodes and properties, U-Boot uses the same bindings as Linux.
A `u-boot.dtsi` file helps to keep U-Boot-specific changes in separate files,
making it easier to keep devicetree source files in U-Boot in sync with Linux.

As a counter-example, the Zephyr OS project takes a different approach. It uses
entirely different bindings, in general, making no effort to sync devicetree
source files with Linux. U-Boot strives to be compatible with Linux in a number
of ways, such as source code style and common APIs, to aid porting of code
between the projects. Devicetree is another way where U-Boot and Linux follow a
similar approach.

Fundamentally, the idea that U-Boot cannot have its own tags flies in the face
of the devicetree specification (see dtspec_), which says:

  Nonstandard property names should specify a **unique string prefix**, such as
  a stock ticker symbol, identifying the name of the company **or organization**
  that defined the property. Examples:

  - fsl,channel-fifo-len
  - ibm,ppc-interrupt-server#s
  - **linux**,network-index

It is also fundamentally unbalanced. Linux has many tags of its own (some 36 in
version 5.13) and at least one Linux-specific node, even if you ignore things
like flash partitions which clearly provide configuration information to Linux.

Practically speaking there are many reasons why U-Boot has its own nodes and
properties. Some examples:

- Binding every device before relocation even if it won't be used, consumes time
  and memory: tags on each node can specify which are needed in SPL or before
  relocation. Linux has no such constraints.

- Requiring the full clock tree to be up and running just to get the debug UART
  running is inefficient. It is also and self-defeating, since if that much
  code is working properly, you probably don't need the debug UART. A devicetree
  property to provide the UART input-clock frequency is a simple solution.

- U-Boot does not have a user space to provide policy and configuration. It
  cannot do what Linux does and run programs and look up filesystems to figure
  out how to boot.


Why not have two devicetrees?
-----------------------------

Setting aside the argument for restricting U-Boot from having its own nodes and
properties, another idea proposed is to have two devicetrees, one for the
U-Boot-specific bits (here called `special`) and one for everything else (here
called `linux`). This would mean that U-Boot would be controlled by two
devicetrees, i.e. OF_CONTROL would require/allow two devicetrees in order to
work.

On the positive side, it might quieten the discussion alluded to in the section
above. But there are many negatives to consider and many open questions to
resolve.

- **Bindings** - Presumably the special devicetree would have its own bindings.
  It would not be necessary to put a `u-boot,` prefix on anything. People coming
  across the devicetree source would wonder how it fits in with the Linux
  devicetree.

- **Access** - U-Boot has a nice `ofnode` API for accessing the devicetree. This
  would need to be expanded to support two trees. Features which need to access
  both (such as a device driver which reads the special devicetree to get some
  configuration info) could become quite confusing to read and write.

- **Merging** - Can the two devicetree be merged if a platform desires it? If
  so, how is this managed in tooling? Does it happen during the build, in which
  case they are not really separate at all. Or does U-Boot merge them at
  runtime, in which case this adds time and memory?

- **Efficiency** - A second device tree adds more code and more code paths. It
  requires that both be made available to the code in U-Boot, e.g. via a
  separate pointer or argument or API. Overall the separation would certainly
  not speed up U-Boot, nor decrease its size.

- **Source code** - At present `u-boot.dtsi` files provide the pieces needed for
  U-Boot for a particular board. Would we use these same files for the special
  devicetree?

- **Complexity** - Two devicetrees complicates the build system since it must
  build and package them both. Errors must be reported in such a way that it
  is obvious which one is failing.

- **Referencing each other** - The `u-boot,dm-xxx` tags used by driver model
  are currently placed in the nodes they relate to. How would these tags
  reference a node that is in a separate devicetree? What extra validation would
  be needed?

- **Storage** - How would the two devicetrees be stored in the image? At present
  we simply concatenate the U-Boot binary and the devicetree. We could add the
  special devicetree before the Linux one, so two are concatenated, but it is
  not pretty. We could use binman to support more complex arrangements, but only
  some boards use this at present, so it would be a big change.

- **API** - How would another project provide two devicetree files to U-Boot at
  runtime? Presumably this would just be too painful. But if it doesn't, it
  would be unable to configure run-time features of U-Boot during the boot.

- **Confusion** - No other project has two devicetrees used for controlling its
  operation (although having multiple devicetrees to pass on to the OS is
  common). U-Boot would be in the unfortunate position of having to describe
  the purpose of the two control devicetrees fact to new users, along with the
  (arguably contrived) reason for the arrangement.

- **Signing flow** - The current signing flow is simple as it involves running
  `mkimage` with the U-Boot devicetree. This would have to be updated to use the
  special devicetree. Some way of telling the user that they have done it wrong
  would have to be invented.

Overall, adding a second devicetree would create enormous confusion and
complexity. It seems a lot cheaper to solve this by a change of attitude.


.. _`TF-A`: https://www.trustedfirmware.org/projects/tf-a
.. _`QEMU ARM`: https://github.com/qemu/qemu/blob/master/hw/arm/virt.c
.. _`QEMU RISC-V`: https://github.com/qemu/qemu/blob/master/hw/riscv/virt.c
.. _`/chosen node`: https://www.kernel.org/doc/Documentation/devicetree/bindings/chosen.txt
.. _fdt_add_pubkey: https://patchwork.ozlabs.org/project/uboot/list/?series=157843&state=*
.. _dtspec: https://www.devicetree.org/specifications/
