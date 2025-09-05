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
    make -j$(nproc)

This produces two files:

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

To disable creation of the library, provide NO_LIBS=1 in the environment::

    NO_LIBS=1 make -j$(nproc)

Simple test programs are provided for each library:::

    # dynamic linking
    $ LD_LIBRARY_PATH=. ./test/ulib/ulib_test
    Hello, world

    - U-Boot

    PS: This program is dynamically linked (uses libu-boot.so)

    # static linking
    $ ./test/ulib/ulib_test_static
    Hello, world

    - U-Boot

    PS: This program is statically linked (uses libu-boot.a)


Architecture Notes
------------------

Both libraries exclude ``arch/sandbox/cpu/main.o`` which contains the
``main()`` function. This allows the linking program to provide its own
main entry point while still using U-Boot functionality through the
``sandbox_main()`` function.

The libraries preserve U-Boot's linker lists, which are essential for
driver registration and other U-Boot subsystems. When using the static
library, use a linker script and ``--whole-archive`` to ensure all linker lists
are included.

Simplified API
--------------

For simple programs that don't need full access to U-Boot internals, a simplified
API is provided through ``<ulib.h>``. This hides the complexity of global data
management and initialization.

Simple Example
~~~~~~~~~~~~~~

.. code-block:: c

    #include <stdio.h>
    #include <ulib.h>

    int main(int argc, char *argv[])
    {
        /* Simple one-line initialization */
        if (ulib_simple_init(argv[0]) < 0) {
            fprintf(stderr, "Failed to initialize U-Boot library\n");
            return 1;
        }

        printf("Hello from U-Boot library!\n");
        printf("Library version: %s\n", ulib_get_version());

        ulib_cleanup();
        return 0;
    }

Building with the Simplified API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using the provided build script::

    cd examples/api
    ./build.sh simple.c
    LD_LIBRARY_PATH=/tmp/b/sandbox ./simple

Or manually with gcc (simple approach)::

    gcc -o myapp myapp.c -L/tmp/b/sandbox -lu-boot -Wl,-rpath,/tmp/b/sandbox
    LD_LIBRARY_PATH=/tmp/b/sandbox ./myapp

The key is to:
1. Avoid including U-Boot headers that conflict with system headers
2. Forward declare only the functions you need
3. Link against libu-boot.so and set the library path

Example Usage
-------------

Please see example/ulib for examples.

Header files
------------

U-Boot has many header files, some of which are arch-specific. These are
typically including via::

    #include <asm/...>

and are located in the ```arch/<arch>/include/asm/...`` directory within the
U-Boot source tree. You will need to ensure that this directory is present in
the include path.


Kconfig options
---------------

There is currently no mechanism to make use of the Kconfig options used to
build the U-Boot library. This will be addressed with future work.


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
* Network subsystem initialization is disabled in library mode
* Main loop functionality is disabled to prevent interference with the host program
* EFI runtime services and relocation are disabled

Future Work
-----------

* Support for other architectures beyond sandbox
* Selective inclusion of U-Boot subsystems
* API versioning and stability guarantees
* pkg-config support for easier integration
