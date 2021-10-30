.. SPDX-License-Identifier: GPL-2.0+

Standard Passage
================

Introduction
------------

It is sometimes necessary for SPL to communicate information to U-Boot proper,
such as the RAM size. This can sometimes be handled by adding the value to a
Kconfig which both SPL and U-Boot proper can use. But this does not work for
values which are detected at runtime.

In some cases other firmware binaries are used alongside U-Boot and these may
need to pass information to U-Boot or receive information from it. In this case
there is no shared build system and it is clumsy so have to specify matching
build options across projects.

U-Boot provides a standard way of passing information between different phases
(TPL, SPL, U-Boot). This is called `standard passage` since it creates a
standard passage through which any sort of information can flow.


How it works
------------

The standard passage is very simple. It is really just a way of sending a
bloblist between programs, either at a fixed address, or using registers to
indicate the location.

A :doc:`bloblist` is a simple, contiguous data structure containing a number of
blobs. Each blob has a tag to indicate what it contains. It is designed for
simple information, like a small C struct. For more complex data, a devicetree
is preferred since it has bindings and is extensible.

The bloblist is typically set up initially by one of the early phases of U-Boot,
such as TPL. It starts either at a fixed address or is allocated in memory using
malloc(). After that, TPL passes the location of the bloblist to SPL (using
machine register in an architecture-specific way) and SPL passes it to U-Boot
proper. It is possible to add new blobs to the bloblist at each phase. U-Boot
proper relocates the bloblist so can expand it if desired.


Use by other projects
---------------------

The standard passage is also intended to be used by other firmware projects,
particularly if they interface with U-Boot. It allows that project's firmware
binaries to pass information to U-Boot (if they run before U-Boot) or receive
information from U-Boot (if they run afterwards).

These projects can copy and modify the bloblist code provided they have a
compatible license.


Allocating tags
---------------

Tags are defined in the `bloblist.h` header file. For the moment, the U-Boot
tree is the upstream repository for tags.

Tags may be allocated in the following areas:

BLOBLISTT_AREA_FIRMWARE_TOP
    A small area for tags of considerable relevance to multiple projects

BLOBLISTT_AREA_FIRMWARE
    A larger area for tags likely to be relevant to multiple projects either now
    or in the future

BLOBLISTT_PROJECT_AREA
    Used for specific projects that want to make sure their tags are correctly
    ignored by other binaries in the firmware flow. This area should not be
    used for tags that are used by multiple projects. Instead, use
    `BLOBLISTT_AREA_FIRMWARE`.

BLOBLISTT_VENDOR_AREA
    Used for specific vendors that want to make sure their tags are correctly
    ignored by other binaries in the firmware flow. This area should not be
    used for tags that are used by multiple vendors. Instead, use
    `BLOBLISTT_AREA_FIRMWARE`.

BLOBLISTT_PRIVATE_AREA
    Used for private flags. Do not send patches with these. They are for local
    or temporary use. Standard firmware binaries which see these tags in the
    bloblist typically refuse to boot.

To add a new tag for your project, send a patch to the U-Boot project with:

  - your new tag, using the next available number in the area your choose
  - a header file in include/stdpass/ containing your struct definition if your
    struct is not actually used in U-Boot
  - a line of code in `board/sandbox/stdpass_check.c` to use the struct (see
    that file for instructions)

The struct definition does not need to match the code style or types names used
in the other project. For example, your project might use a type like
__UNSIGNED_INT32 which in U-Boot would be written as u32. Types should be sized
so that the struct is the same on 32- and 64-bit machines. Avoid using __packed
if possible. Instead try to order members so that it is not necessary.

Conflicts are resolved before applying patches to mainline, so your actual tag
value may change when the patch is applied. Once your patch is accepted your tag
is allocated for all time.

Devicetree
----------

Devicetree has a special place in the standard passage. One of the bloblist tags
is BLOBLISTT_CONTROL_DTB which indicates that that blob contains a devicetree
intended to control U-Boot (or other binaries). This devicetree provides
hardware information and configuration in a generic way using standard bindings,
so that it is available for any project to use. The bindings are compatible with
operating systems (including Linux) so there is no need to remove them before
calling the OS.

In cases where a binary wants to access the devicetree but does not want to
implement the bloblist feature, the offset of the devicetree within the
bloblist is provided. This avoids the need to implement bloblists just to
access the devicetree. This is a convenience feature intended for use in
degenerate cases.

However U-Boot itself does not permit accepting a devicetree through standard
passage unless it is part of a valid bloblist. It is easy to turn on the
bloblist feature in U-Boot, so such a variation would only serve to confuse
things and encourage degeneration in other projects.


Standard passage API
--------------------

The protocol for passing a bloblist through the standard passage from one
binary to another is architecture-specific, meaning it works differently on
32-/64-ARM and x86, for example, if only because of the different register
naming.

Two registers are used, the same ones as are used for passing the first two
arguments to a C function using the architecture's procedure-call standard.
For example, on 32-bit ARM this is:

  =========  ============================
  Register   Contents
  =========  ============================
  r0         Address of bloblist
  r1         Offset of devicetree from r0
  =========  ============================

The devicetree is provided as an offset since it must be included within the
bloblist. It is not permitted to pass a devicetree except in a bloblist, if
standard passage is used. Boards which require some other arrangement will have
to use some other, custom arrangement, not standard passage. No variations or
exceptions are possible, since that would defeat the purpose of a standard.


Usage guidelines
----------------

As mentioned above, blobs should contain small, simple blocks of information,
typically represented by a C structure. Using it for large or complex structures
is only permitted if these are defined by a standard byte-accurate form.
Examples include devicetree, ACPI tables, SMBIOS tables and the like.
There are also a lot of pre-existing firmware binaries which are quite complex
but qualify because it is not possible to convert them to devicetree now. Apart
from those exceptions, keep it simple!

For complex data structures, devicetree can be used. The libfdt library has an
overhead of around 5-10KB which is small enough that most firmware binaries can
easily incorporate it. For those that must run in very constrained environments,
like U-Boot TPL, a simple blob can be used instead, as explained in the
preceding paragraph.

Devicetree bindings must be defined so that the format of the data is well
understood. This is done through the `dt-schema`_ project, although this process
is still in its infancy.


Updates
-------

Updates and patches to this documentation are welcome. Please submit them to
the U-Boot project in the normal way.


Design notes
------------

This section describes some of the reasons behind the design decisions implied
by this feature.

Why devicetree?
~~~~~~~~~~~~~~~

Firmware is getting large and complicated, with a concomitant need for more
complex communication between binaries. We don't want to use C structs to pass
around complex data, nor invent new binary formats for everything that comes up.
The devicetree provides a useful format and is already widely used in firmware.
It supports bindings and provides validation to check for compliance, by virtue
of the Linux project and `dt-schema`_. It is easily extensible, being basically
an efficient, hierarchical, ordered dictionary.

Some examples of how complex and annoying binary formats can become are SMBIOS
tables and Intel's Video BIOS tables. The world does not need another binary
format.


Why not *just* devicetree?
~~~~~~~~~~~~~~~~~~~~~~~~~~

Some early firmware binaries run in very little memory and only need to pass a
few values on to later phases. Devicetree is too heavy-weight for these cases.
For example, it is generally not possible for TPL to access a devicetree, which
is one of the motivations for the of-platdata feature.


Why not protobuf, YAML, JSON?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

They are not as efficient and either use a lot more code or require parsing
before use. Devicetree happens to be a nice compromise.


Why not something else?
~~~~~~~~~~~~~~~~~~~~~~~

Possibly. Please propose it.


Why not UUIDs?
~~~~~~~~~~~~~~

Why use a simple integer tag instead of an 'industry-standard' UUID? Many
reasons:

- Code should be as readable as possible. GUIDs require something like::

    EFI_GUID(0x6dadf1d1, 0xd4cc, 0x4910, \
        0xbb, 0x6e, 0x82, 0xb1, 0xfd, 0x80, 0xff, 0x3d)

  which is much harder to read than::

    enum {
        BLOBLISTT_SPL_HANDOFF = 123,
    };

- UUIDs are more like a hash than a sequence number. Git uses them, although a
  short form of the hash is commonly shown. Why use a hash to identify
  something when we only have a small number of items?

- We don't need to worry about collisions in open source software. We can have
  a shared repo and allocate sequence numbers from there. UUIDs come from the
  idea that everyone is working independently so people need to be able to
  allocate their own numbers and be sure that they will not conflict. This is
  needed in the PC BIOS industry where there is little shared source /
  cooperation. It is not helpful with open source.

- UUIDs come across as just obfuscation. Does anyone know what these values
  mean? How would we look them up? Who owns which one? Is there a central
  registry?::

    EFI_GUID(0x721acf02, 0x4d77, 0x4c2a, 0xb3, 0xdc, 0x27, 0x0b, 0x7b, 0xa9, 0xe4, 0xb0)
    EFI_GUID(0xa034147d, 0x690c, 0x4154, 0x8d, 0xe6, 0xc0, 0x44, 0x64, 0x1d, 0xe9, 0x42)
    EFI_GUID(0xbbcff46c, 0xc8d3, 0x4113, 0x89, 0x85, 0xb9, 0xd4, 0xf3, 0xb3, 0xf6, 0x4e)
    EFI_GUID(0x69a79759, 0x1373, 0x4367, 0xa6, 0xc4, 0xc7, 0xf5, 0x9e, 0xfd, 0x98, 0x6e)
    EFI_GUID(0xd038747c, 0xd00c, 0x4980, 0xb3, 0x19, 0x49, 0x01, 0x99, 0xa4, 0x7d, 0x55)
    EFI_GUID(0x9c7c3aa7, 0x5332, 0x4917, 0x82, 0xb9, 0x56, 0xa5, 0xf3, 0xe6, 0x2a, 0x07)

- It is overkill to use 16 bytes for a unique identifier in a shared project.
  It is about 10^38. Modern SoCs cannot keep that in a register and there is no
  C int type to represent it on most common hardware today! Having to check that
  adds time and code to no benefit. In early boot, space and time are
  particularly precious.


Why contiguous?
~~~~~~~~~~~~~~~

It is easier to allocate a single block of data than to allocate lots of little
blocks. It is easy to relocate if needed (a simple copy of a block of memory).
It can be expanded by relocating it. If we absolutely need to create a linked
list then pointers to external data can be added to a blob.


Why bloblist?
~~~~~~~~~~~~~

Bloblist is a simple format with an integer tag. It avoids UUIDs and meets the
requirements above. Some tweaks may be desirable to the format, but that can be
worked out in code review.


Why pass the devicetree offset?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In cases where a binary wants to access the devicetree but does not want to
implement the bloblist feature, the offset of the devicetree within the
bloblist is provided. This avoids the need to implement bloblists just to
access the devicetree. This is a convenience feature intended for use in
degenerate cases.


Why use registers to pass values between binaries?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It seems like the obvious solution. We could use a shared memory region with
shared configuration between projects, but that is error prone and difficult to
keep in sync.


Why not add magic values to indicate that standard passage is used?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We could put a magic value in a register to tell the next phase that the
standard-passage information is available (in other registers). But making it
a build-time option saves at least one register and makes things a little more
deterministic at built time. If we know we can rely on it, it is easier to
use.


.. _`dt-schema`: https://github.com/devicetree-org/dt-schema
