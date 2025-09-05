.. SPDX-License-Identifier: GPL-2.0+

U-Boot Library (ulib)
=====================

The U-Boot Library (ulib) allows U-Boot to be built as a shared or static
library that can be used by external programs. This enables reuse of U-Boot
functionality in test programs and other applications without needing to
build that functionality directly into U-Boot image.

Please read `License Implications`_ below.

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

Building outside the U-Boot tree
--------------------------------

This is possible, but as soon as you want to call a function that is not in
u-boot-lib.h you will have problems, as described in the following sections.

This will be addressed with future work.

With that caveat, see example/ulib/README for instructions on how to use the
provided example.

Including U-Boot header files from outside
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

U-Boot has many header files, some of which are arch-specific. These are
typically including via::

    #include <asm/...>

and are located in the ```arch/<arch>/include/asm/...`` directory within the
U-Boot source tree. You will need to ensure that this directory is present in
the include path.


Kconfig options
~~~~~~~~~~~~~~~

There is currently no mechanism to make use of the Kconfig options used to
build the U-Boot library. It is possible to add `-include kconfig.h` to your
build, but for this to work more generally, the header file containing the
CONFIG settings would need to be exported from the build and packaged with the
library.


Test Programs
-------------

U-Boot includes test programs that demonstrate library usage:

* ``test/ulib/ulib_test`` - Uses the shared library
* ``test/ulib/ulib_test_static`` - Uses the static library

These are built by default with the sandbox build.

Run the shared library version::

    ./test/ulib/ulib_test

Run the static library version::

    ./test/ulib/ulib_test_static

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

When linking with the U-Boot library for sanbod, you may need these system
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

Future Work
-----------

* Support for other architectures beyond sandbox
* Selective inclusion of U-Boot subsystems
* API versioning and stability guarantees
* pkg-config support for easier integration
* Support for calling functions in any U-Boot header
