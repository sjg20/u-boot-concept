.. SPDX-License-Identifier: GPL-2.0+

U-Boot Library (ulib)
=====================

The U-Boot Library (ulib) allows U-Boot to be built as a shared or static
library that can be used by external programs. This enables reuse of U-Boot
functionality in test programs and other applications without needing to
build that functionality directly into a U-Boot image.

Please read `License Implications`_ below.

Motivation
----------

U-Boot contains a vast arrange of functionality. It supports standard boot,
native execution (sandbox) for development and testing, filesystems, networking,
all sorts of boot protocols, drivers and support for about 1300 boards, a full
command line interface, a configuration editor / graphical menu, good Linux
compatibility for porting drivers, a powerful but efficient driver model which
uses Linux devicetree and many other features. The code base is fairly modern,
albeit with some dark spaces. Unusually for firmware, U-Boot provides a vast
array of tests. It can boot EFI apps or as an EFI app. It supports most relevant
architectures and modern SoCs.

But of course time marches on and innovation must continue. U-Boot will clearly
be part of the picture in the future, but what is next?

Ulib is an attempt to make U-Boot's functionality more easily available to other
projects, so they can build on it improve it or even replace parts of it. Ulib
aims to open up the capabilities of U-Boot to new use cases.

Ulib also provides the ability to write the main program in another language.
For now C and Rust are supported, but Python should also be possible, albeit
with a significant amount of work.

Few can predict where boot firmware will be in 10 years. The author of this file
rashly believes that we may have moved on from U-Boot, EFI and many other things
considered essential today. Perhaps firmware will be written in Rust or Zig or
Carbon or some other new language. Our AI overlords may be capable of writing
firmware based on a specification, if we can feed them enough electricity. Or it
could be that the complexity of SoCs grows at such a rate that we just carry on
as we are, content to just be able to make something boot.

Ulib aims to provide a bridge from the best (more or less) of what we have today
to whatever it is that will replace it. Ulib is not an end itself, just a
platform for further innovation in booting, to new languages, new boot protocols
and new development methods.


Building the Libraries
----------------------

For now the library is only available for :doc:`/arch/sandbox/sandbox`, i.e. for
running natively on a host machine. Further work will extend that to other
architectures supported by U-Boot.

To build U-Boot as a library (libu-boot.so)::

    make sandbox_defconfig
    make [-s] -j$(nproc)

Use ``-s`` if you just want to see warnings / errors. This produces two files:

``libu-boot.so``
    This is the shared library, sometimes called a dynamically linked library.
    Programs which need it can be dynamically linked with this library at
    runtime. This provides:

        - Smaller executable size for linked programs
        - Runtime dependency on the .so file
        - Must set LD_LIBRARY_PATH or use rpath for runtime linking
        - Suitable for development and testing

``libu-boot.a``
    This is a static library, meaning that it is directly linked with your
    program and the resulting executable includes the U-Boot code. This
    provides:

        - Larger executable size (includes all code)
        - No runtime dependencies
        - Self-contained executables
        - Suitable for distribution and embedded systems

To disable creation of the library (e.g. to speed up the build), provide
NO_LIBS=1 in the environment::

    NO_LIBS=1 make -j$(nproc)

Simple test programs are provided for each library.

For dynamic linking::

    $ LD_LIBRARY_PATH=. ./test/ulib/ulib_test
    Hello, world

    - U-Boot

    PS: This program is dynamically linked (uses libu-boot.so)

For static linking::

    $ ./test/ulib/ulib_test_static
    Hello, world

    - U-Boot

    PS: This program is statically linked (uses libu-boot.a)


Architecture Notes
------------------

Both libraries exclude ``arch/sandbox/cpu/main.o`` which contains the
``main()`` function. This allows the linking program to provide its own
main entry point while still using U-Boot functionality.

The libraries preserve U-Boot's linker lists, which are essential for
driver registration and other U-Boot subsystems.

**Link Time Optimization (LTO) Compatibility**

When building with ``CONFIG_ULIB=y``, Link Time Optimization (LTO) is
automatically disabled. This is because the symbol renaming process uses
``objcopy --redefine-sym``, which is incompatible with LTO-compiled object
files. The build system handles this automatically.

Building outside the U-Boot tree
--------------------------------

This is possible using the provided examples as a template. The ``examples/ulib``
directory contains a standalone Makefile that can build programs against a
pre-built U-Boot library.

The examples work as expected, but note that as soon as you want to call
functions that are not in the main API headers, you may have problems with
missing dependencies and header files. See below.

See the **Example Programs** section above for build instructions.

Including U-Boot header files from outside
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

U-Boot has many header files, some of which are arch-specific. These are
typically included via::

    #include <asm/...>

and are located in the ``arch/<arch>/include/asm/...`` directory within the
U-Boot source tree. You will need to ensure that this directory is present in
the include path.


Kconfig options
~~~~~~~~~~~~~~~

There is currently no mechanism to make use of the Kconfig options used to
build the U-Boot library. It is possible to add `-include kconfig.h` to your
build, but for this to work more generally, the header file containing the
CONFIG settings would need to be exported from the build and packaged with the
library.


Test Programs and Examples
--------------------------

U-Boot includes several test programs and examples that demonstrate library
usage:

**Test Programs**

* ``test/ulib/ulib_test`` - Uses the shared library
* ``test/ulib/ulib_test_static`` - Uses the static library

These are built by default with the sandbox build.

Run the shared library version::

    ./test/ulib/ulib_test

Run the static library version::

    ./test/ulib/ulib_test_static

**Example Programs**

The ``examples/ulib`` directory contains more complete examples:

* ``demo`` - Dynamically linked demo program showing U-Boot functionality
* ``demo_static`` - Statically linked version of the demo program

These examples demonstrate:

* Proper library initialization with ``ulib_init()``
* Using U-Boot OS functions like ``os_open()``, ``os_fgets()``, ``os_close()``
* Using renamed U-Boot library functions via ``u-boot-api.h``
  (e.g., ``ub_printf()``)
* Multi-file program structure (``demo.c`` + ``demo_helper.c``)
* Proper cleanup with ``ulib_uninit()``

To build and run the examples::

    # Make sure U-Boot itself is built
    make O=/tmp/b/sandbox sandbox_defconfig all

    cd examples/ulib
    make UBOOT_BUILD=/tmp/b/sandbox srctree=../..
    ./demo_static

**Building Examples Outside U-Boot Tree**

The examples can be built independently if you have a pre-built U-Boot library::

    cd examples/ulib
    make UBOOT_BUILD=/path/to/uboot/build srctree=/path/to/uboot/source

The Makefile supports both single-file and multi-object programs through the
``demo-objs`` variable. Set this to build from multiple object files, or leave
empty to build directly from source.

Rust Examples
-------------

U-Boot also includes Rust examples that demonstrate the same functionality using
the ``u-boot-sys`` crate:

**Rust Demo Program**

The ``examples/rust`` directory contains Rust examples:

* ``demo`` - Dynamically linked Rust demo program
* ``demo_static`` - Statically linked Rust version

These Rust examples demonstrate:

* Using the ``u-boot-sys`` crate for FFI bindings
* Proper library initialization with ``ulib_init()``
* Using U-Boot OS functions like ``os_open()``, ``os_fgets()``, ``os_close()``
* Using renamed U-Boot library functions (e.g., ``ub_printf()``)
* Modular program structure with helper functions in ``rust_helper.rs``
* Proper cleanup with ``ulib_uninit()``

**Building Rust Examples**

To build and run the Rust examples::

    # Make sure U-Boot itself is built
    make O=/tmp/b/sandbox sandbox_defconfig all

    cd examples/rust
    make UBOOT_BUILD=/tmp/b/sandbox srctree=../..
    ./demo_static

Or using Cargo directly::

    cd examples/rust
    env UBOOT_BUILD=/tmp/b/sandbox cargo build --release --bin demo
    LD_LIBRARY_PATH=/tmp/b/sandbox ./target/release/demo

**Rust Crate Structure**

The Rust examples use the ``u-boot-sys`` crate located in ``lib/rust/``, which provides:

* FFI bindings for U-Boot library functions (``ulib_*``)
* FFI bindings for U-Boot API functions (``ub_*``)  
* FFI bindings for OS abstraction functions (``os_*``)
* Proper Rust documentation and module organization

The crate follows Rust ``*-sys`` naming conventions for low-level FFI bindings.

**Building Rust Examples Outside U-Boot Tree**

The Rust examples can be built independently using the ``u-boot-sys`` crate::

    cd examples/rust
    env UBOOT_BUILD=/path/to/uboot/build cargo build --release

The examples demonstrate both static and dynamic linking approaches compatible
with the Rust toolchain.

Linking and the Linker Script
-----------------------------

For the static library, a custom linker script is needed to ensure proper
section alignment, particularly for U-Boot linker lists. See
``arch/sandbox/cpu/ulib-test-static.lds`` for an example.

The linker script ensures:

* Proper alignment of the ``__u_boot_list`` section (32-byte alignment)
* Correct ordering of sandbox-specific sections
* Preservation of linker list entries with ``KEEP()`` directives

For this to work, the library must be linked using a 'whole archive' approach,
for example::

    -Wl,--whole-archive $(obj)/libu-boot.a -Wl,--no-whole-archive

Failure to do either of these will result in strange errors, such as running
out of memory or simple crashes during library init.

Dependencies
------------

When linking with the U-Boot library for sandbox, you may need these system
libraries:

* ``pthread`` - POSIX threads
* ``dl`` - Dynamic linking support
* ``SDL2`` - For sandbox display emulation (optional)
* ``rt`` - Real-time extensions

Troubleshooting
---------------

Missing Symbols
~~~~~~~~~~~~~~~

If you encounter undefined symbol errors when linking:

1. For static library, ensure you're using ``--whole-archive``
2. Check that required system libraries are linked
3. Some symbols may need to be defined with ``--defsym``:

   - ``__stack_chk_guard`` - Stack protection guard
   - ``sandbox_sdl_sync`` - SDL synchronization (can be set to 0 if unused)

Linker List Issues
~~~~~~~~~~~~~~~~~~

If U-Boot subsystems don't initialize properly:

1. Check linker list alignment with::

       scripts/check_linker_lists.py /path/to/executable

2. For static linking, use ``--whole-archive`` to include all sections
3. Use a custom linker script for proper section organization

Runtime Errors
~~~~~~~~~~~~~~

For shared library programs:

1. Ensure ``LD_LIBRARY_PATH`` includes the directory with ``libu-boot.so``
2. Or use ``-Wl,-rpath`` when linking to embed the library path
3. Check library dependencies with ``ldd myapp``


License Implications
--------------------

U-Boot is licensed under GPL-2.0+ (GNU General Public License version 2 or later).
This has important implications when linking against the U-Boot library:

* **Static Linking (libu-boot.a)**:

  - Your program becomes a derivative work of U-Boot
  - The entire combined work must be distributed under GPL-2.0+ terms
  - You must provide source code for your entire application
  - All code linked with the static library must be GPL-compatible

* **Dynamic Linking (libu-boot.so)**:

  - The GPL interpretation for dynamic linking is legally complex
  - Conservative interpretation: still creates a derivative work requiring GPL
  - Some jurisdictions may treat dynamic linking differently
  - Consult legal counsel for commercial applications

* **Compliance Requirements**:

  - Provide a GPL-2.0+ license notice
  - Make source code available (including your modifications)
  - Include copyright notices from U-Boot
  - Provide build instructions to reproduce the binary

* **Alternative Approaches**:

  - Consider using U-Boot functionality via separate processes (IPC)
  - Implement a clean-room alternative for needed functionality
  - Request dual-licensing from all U-Boot contributors (impractical)

**Note**: This is not legal advice. Always consult with legal professionals
when using GPL-licensed code in your products, especially for commercial use.

Limitations
-----------

* Currently only supported for sandbox architecture
* Network-subsystem init is disabled in library mode
* Main-loop functionality is disabled to prevent interference with the host
  program
* EFI runtime-services and relocation are disabled

Symbol Renaming and API Generation
-----------------------------------

U-Boot includes a build script (``scripts/build_api.py``) that supports symbol
renaming and API-header generation for library functions. This allows creating
namespaced versions of standard library functions to avoid conflicts.

For example, when linking with the library, printf() refers to the stdio
printf() function, while ub_printf() refers to U-Boot's version.

Build System Integration
~~~~~~~~~~~~~~~~~~~~~~~~

The symbol renaming system is automatically integrated into the U-Boot build
process:

* The symbol definitions are stored in ``lib/ulib/rename.syms``
* During the sandbox build, the build system automatically:

  - Renames symbols in object files using ``--redefine`` when building the
    U-Boot libraries (``libu-boot.so`` and ``libu-boot.a``)
  - Generates ``include/u-boot-api.h`` with renamed function declarations
    using ``--api``

* The API header provides clean interfaces for external programs linking
  against the U-Boot library
* Symbol renaming ensures no conflicts between U-Boot functions and system
  library functions

Symbol Definition File Format
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The script uses a symbol definition file (``rename.syms``) with this format::

    # Comment lines start with #
    file: stdio.h
     printf
     sprintf=ub_sprintf_custom

    file: string.h
     strlen
     strcpy

The format rules are:

* Lines starting with ``file:`` specify a header file
* Indented lines (space or tab) define symbols from that header
* Use ``symbol=new_name`` for custom renaming, otherwise a ``ub_`` prefix is
  added by default. No space around ``=``
* Use ``#`` at the beginning of a line for a comment
* Empty lines are allowed

Script Usage
~~~~~~~~~~~~

The build script provides several functions:

**Parse and display symbols**::

    python scripts/build_api.py rename.syms --dump

**Apply symbol renaming to object files**::

    python scripts/build_api.py rename.syms \
        --redefine file1.o file2.o \
        --output-dir /tmp/renamed_objects

**Generate API header with renamed functions**::

    python scripts/build_api.py rename.syms \
        --api ulib_api.h \
        --include-dir /path/to/headers \
        --output-dir /tmp/objects

Script Architecture
~~~~~~~~~~~~~~~~~~~

The build script consists mostly of these classes:

* **RenameSymsParser**: Parse the symbol definition file format and validate
  syntax
* **DeclExtractor**: Extract function declarations with comments from headers
* **SymbolRedefiner**: Apply symbol renaming to object files using ``objcopy``
* **ApiGenerator**: Create unified API headers with renamed function
  declarations

Symbol renaming operations copy files to an output directory rather than
modifying them in-place, to avoid race conditions.

Object File Processing
~~~~~~~~~~~~~~~~~~~~~~

When processing object files, the script:

1. Uses ``nm`` to check which files contain target symbols
2. Copies unchanged files that don't contain target symbols
3. Applies ``objcopy --redefine-sym`` for files needing renaming
4. Creates unique output filenames by replacing path separators with
   underscores

API Header Generation
~~~~~~~~~~~~~~~~~~~~~

The API header generation process:

1. Groups symbols by their source header files
2. Searches for original header files in the specified include directory
3. Extracts function declarations (including comments) from source headers
4. Applies symbol renaming to the extracted declarations
5. Combines everything into a single API header file

If any required headers or function declarations are missing, the script fails
with detailed error messages listing exactly what couldn't be found.

Future Work
-----------

* Support for other architectures beyond sandbox
* Selective inclusion of U-Boot subsystems
* API versioning and stability guarantees
* pkg-config support for easier integration
* Support for calling functions in any U-Boot header, without needing the source
  tree
* Improved symbol renaming with namespace support
* Expanded features in the lib/rust library
