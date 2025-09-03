.. SPDX-License-Identifier: GPL-2.0+

U-Boot Library (ulib)
=====================

The U-Boot Library (ulib) allows U-Boot to be built as a shared or static
library that can be used by external programs. This enables reuse of U-Boot
functionality in test programs and other applications without needing to
build a complete U-Boot image.

Building the Libraries
----------------------

Shared Library
~~~~~~~~~~~~~~

To build U-Boot as a shared library (libu-boot.so)::

    make sandbox_defconfig
    make -j$(nproc) libu-boot.so

This produces ``libu-boot.so`` in the build directory, which can be
dynamically linked with other programs.

Static Library
~~~~~~~~~~~~~~

To build U-Boot as a static library (libu-boot.a)::

    make sandbox_defconfig
    make -j$(nproc) libu-boot.a

This produces ``libu-boot.a`` in the build directory, which can be
statically linked with other programs.

Key Differences
---------------

The main differences between the shared and static libraries:

* **Shared library (libu-boot.so)**:
  
  - Smaller executable size for linked programs
  - Runtime dependency on the .so file
  - Must set LD_LIBRARY_PATH or use rpath for runtime linking
  - Suitable for development and testing

* **Static library (libu-boot.a)**:
  
  - Larger executable size (includes all code)
  - No runtime dependencies
  - Self-contained executables
  - Suitable for distribution and embedded systems

Architecture Notes
------------------

Both libraries exclude ``arch/sandbox/cpu/main.o`` which contains the
``main()`` function. This allows the linking program to provide its own
main entry point while still using U-Boot functionality through the
``sandbox_main()`` function.

The libraries preserve U-Boot's linker lists, which are essential for
driver registration and other U-Boot subsystems. When using the static
library, use ``--whole-archive`` to ensure all linker lists are included.

Example Usage
-------------

Shared Library Example
~~~~~~~~~~~~~~~~~~~~~~

Here's a simple program that uses the shared library:

.. code-block:: c

    #include <u-boot/u-boot.h>
    
    int main(int argc, char *argv[])
    {
        /* Initialize U-Boot subsystems */
        return sandbox_main(argc, argv);
    }

Compile and link with::

    gcc -I/path/to/u-boot/include -o myapp myapp.c \
        -L/path/to/u-boot -lu-boot \
        -Wl,-rpath,/path/to/u-boot

Static Library Example
~~~~~~~~~~~~~~~~~~~~~~

The same program can be linked statically::

    gcc -I/path/to/u-boot/include -o myapp myapp.c \
        -Wl,--whole-archive /path/to/u-boot/libu-boot.a \
        -Wl,--no-whole-archive \
        -lpthread -ldl -lSDL2 -lrt

Note the use of ``--whole-archive`` to ensure all U-Boot linker lists
are included in the final executable.

Test Programs
-------------

U-Boot includes test programs that demonstrate library usage:

* ``test/ulib/ulib_test`` - Uses the shared library
* ``test/ulib/ulib_test_static`` - Uses the static library

Build both with::

    make -j$(nproc) test/ulib/ulib_test test/ulib/ulib_test_static

Run the shared library version::

    ./test/ulib/ulib_test

Run the static library version::

    ./test/ulib/ulib_test_static

Linker Script
-------------

For the static library, a custom linker script may be needed to ensure
proper section alignment, particularly for U-Boot linker lists. See
``arch/sandbox/cpu/ulib-test-static.lds`` for an example.

The linker script ensures:

* Proper alignment of ``__u_boot_list`` sections (32-byte alignment)
* Correct ordering of sandbox-specific sections
* Preservation of linker list entries with ``KEEP()`` directives

Dependencies
------------

When linking with the U-Boot library, you may need these system libraries:

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
3. Consider using a custom linker script for proper section organization

Runtime Errors
~~~~~~~~~~~~~~

For shared library programs:

1. Ensure ``LD_LIBRARY_PATH`` includes the directory with ``libu-boot.so``
2. Or use ``-Wl,-rpath`` when linking to embed the library path
3. Check library dependencies with ``ldd myapp``

Limitations
-----------

* Currently only supported for sandbox architecture
* Network subsystem initialization is disabled in library mode
* Main loop functionality is disabled to prevent interference with the host program
* EFI runtime services and relocation are disabled

Future Work
-----------

* Support for other architectures beyond sandbox
* Selective inclusion of U-Boot subsystems
* API versioning and stability guarantees
* pkg-config support for easier integration