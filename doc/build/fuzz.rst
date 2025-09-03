.. SPDX-License-Identifier: GPL-2.0+

Building U-Boot with Fuzzing Support
=====================================

U-Boot supports fuzzing through libFuzzer when built for the sandbox
architecture. Fuzzing helps identify security vulnerabilities and crashes by
testing with randomly generated inputs.

Prerequisites
-------------

The following tools are required:

* Clang compiler with fuzzing support
* libstdc++ development libraries

On Ubuntu/Debian systems, install the required packages::

    sudo apt install clang libstdc++-dev

Building with Fuzzing
---------------------

The recommended approach is to use buildman, which handles the configuration
automatically:

1. Build with buildman (recommended)::

    buildman --bo sandbox -a FUZZ=y -O clang -L -o /tmp/fuzz -w

The buildman options:

* ``--booard sandbox`` - Build for sandbox board only
* ``-a FUZZ=y`` - Enable fuzzing support via CONFIG_FUZZ=y
* ``-O clang`` - Use Clang compiler (required for fuzzing)
* ``-L`` - Disable LTO to avoid sanitizer coverage linker issues
* ``-o /tmp/fuzz`` - Output directory
* ``-w`` - Use the output directory as the work directory

Alternative: Manual build
~~~~~~~~~~~~~~~~~~~~~~~~~

To build manually with make:

1. Configure the build with fuzzing enabled::

    make HOSTCC=clang CC=clang O=/tmp/fuzz LTO_ENABLE= sandbox_defconfig
    scripts/config --file /tmp/fuzz/.config --enable FUZZ

2. Build the fuzzing-enabled binary::

    make HOSTCC=clang CC=clang O=/tmp/fuzz LTO_ENABLE= -j$(nproc)

Build Output
------------

The fuzzing build produces:

* ``u-boot`` - Main fuzzing binary with AddressSanitizer and fuzzer
  instrumentation
* Significantly larger binary size due to instrumentation (typically 40-50MB)
* Debug symbols included for better crash analysis

Fuzzing Architecture
--------------------

The U-Boot fuzzing implementation consists of:

* **Fuzzing Engine**: Sandbox-specific driver that interfaces with libFuzzer
* **Threading Model**: Separate threads for fuzzing harness and U-Boot
  execution
* **Input Handling**: ``LLVMFuzzerTestOneInput()`` entry point processes
  fuzz inputs
* **Command Fuzzing**: Tests U-Boot commands with generated inputs via
  ``fuzz`` command

Key source files:

* ``arch/sandbox/cpu/fuzz.c`` - Main fuzzing implementation
* ``drivers/fuzz/`` - Fuzzing engine drivers
* ``test/fuzz/`` - Fuzzing test cases
* ``include/fuzzing_engine.h`` - Fuzzing engine interface

Running Fuzz Tests
------------------

To run fuzzing tests, set the test name via environment variable and run the
fuzzing binary from the build directory:

1. Change to the build directory::

    cd /tmp/fuzz

2. Set the fuzz test to run::

    export UBOOT_SB_FUZZ_TEST=fuzz_vring

3. Run the fuzzer::

    ./u-boot

The fuzzer will start libFuzzer with coverage-guided input generation. You
should see output similar to::

    INFO: Running with entropic power schedule (0xFF, 100).
    INFO: Seed: 1626867009
    INFO: Loaded 1 modules   (104150 inline 8-bit counters): ...
    #2      INITED cov: 28 ft: 29 corp: 1/1b exec/s: 0 rss: 318Mb
    #4      NEW    cov: 29 ft: 30 corp: 2/3b lim: 4 exec/s: 0 rss: 319Mb

Available fuzz tests include:

* ``fuzz_vring`` - Tests VirtIO ring buffer handling

To stop fuzzing, use Ctrl+C. The fuzzer will automatically save any crash-
inducing inputs for later analysis.

Understanding Fuzzer Output
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The fuzzer output shows:

* ``cov: N`` - Number of code coverage points reached
* ``ft: N`` - Number of features discovered
* ``corp: N/Mb`` - Corpus size (number of test cases / total bytes)
* ``exec/s: N`` - Executions per second (performance metric)
* ``rss: NMb`` - Memory usage

Error messages from the target code (like VirtIO "out of range" errors) are
expected and indicate the fuzzer is finding edge cases.

Adding New Fuzz Tests
---------------------

To create a new fuzz test, follow these steps:

1. **Create the test file** in ``test/fuzz/`` directory::

    /* SPDX-License-Identifier: GPL-2.0+ */
    #include <test/fuzz.h>

    static int fuzz_my_component(const uint8_t *data, size_t size)
    {
        /* Your fuzzing logic here */
        if (size < 4)
            return 0;  /* Not enough data */

        /* Test your component with fuzzed data */
        my_component_function(data, size);

        return 0;
    }
    FUZZ_TEST(fuzz_my_component, 0);

2. **Add to Makefile** in ``test/fuzz/Makefile``::

    obj-$(CONFIG_MY_COMPONENT) += my_component.o

   Or for tests that should always be included::

    obj-y += my_component.o

3. **Test the new fuzzer**::

    export UBOOT_SB_FUZZ_TEST=fuzz_my_component
    ./u-boot

**Best practices for fuzz tests:**

* **Input validation**: Check minimum data size requirements
* **Error handling**: Handle invalid inputs gracefully, don't panic
* **Resource cleanup**: Free any allocated resources
* **Focused testing**: Target specific functions or code paths
* **Deterministic**: Same input should produce same behavior

**Example patterns:**

* Parse structured data (protocols, file formats)
* Test buffer handling with varying sizes
* Exercise error paths with malformed inputs
* Stress test with boundary conditions

Troubleshooting
---------------

**Linker errors about missing libstdc++**:
  Install libstdc++ development libraries as shown in Prerequisites.

**Sanitizer coverage linker errors**:
  Ensure LTO is disabled with ``LTO_ENABLE=`` in the make command.

**Build fails with GCC**:
  Fuzzing requires Clang. Ensure both CC and HOSTCC are set to clang.

**Fuzzer exits with "fdtdec_setup() failed"**:
  Run the fuzzer from the build directory where u-boot.dtb is located.
  The sandbox requires access to its device tree file.

Security Considerations
-----------------------

Fuzzing builds include:

* **AddressSanitizer**: Detects buffer overflows, use-after-free, and other
  memory errors
* **Fuzzer Coverage**: Instruments code for coverage-guided fuzzing
* **Debug Information**: Retained for crash analysis and debugging

These features significantly increase binary size and runtime overhead, making
fuzzing builds unsuitable for production use.

Further Reading
---------------

* :doc:`/arch/sandbox/sandbox` - General sandbox architecture documentation
* libFuzzer documentation: https://llvm.org/docs/LibFuzzer.html
* AddressSanitizer documentation:
  https://clang.llvm.org/docs/AddressSanitizer.html
