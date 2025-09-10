# Technical Notes - Rust U-Boot Integration

This directory contains a simple Rust implementation for using U-Boot's
library. Both static and dynamic linking (of libu-boot) are available.

This directory is intended to be used separately from U-Boot's build system,
e.g. by copying it somewhere else.

For comprehensive documentation, see :doc:`/doc/develop/ulib`.

## Build System Success

The following components are included:

### 1. u-boot-sys Crate

The `lib/rust/u-boot-sys` crate provides FFI bindings organized into modules:
- `uboot_lib`: Library management functions (`ulib_*`)
- `uboot_api`: U-Boot API functions (`ub_*`)
- `os`: OS abstraction functions (`os_*`)

Functions are re-exported at the crate root for convenience.

### 2. Example Program Structure

- `demo.rs`: Main program using the u-boot-sys crate
- `rust_helper.rs`: Helper functions similar to C demo_helper.c
- Modular design with proper separation of concerns

### 3. Build Configuration
- `build.rs` configures linking with both static and dynamic libraries
- Uses `--whole-archive` for static linking to preserve U-Boot linker lists
- Links required system libraries (pthread, dl, rt, SDL2) with proper ordering
- Uses U-Boot's existing stack-protection implementation from stackprot.c

### 4. Integration
- Makefile can be use standalone or from U-Boot build system
- Cargo project depends on `u-boot-sys` crate from `../../lib/rust`
- Examples are built automatically as part of U-Boot's build system

### 5. Runtime Execution ✅
- `demo` (dynamic) and `demo-static` executables work the same
- Calls library init with `ulib_init()`
- File operations using U-Boot's OS abstraction layer
- Uses renamed U-Boot library functions (`ub_printf`)
- Clean shutdown with `ulib_uninit()`

## Architecture Overview

### u-boot-sys Crate Design
- Located in `lib/rust/` for reuse across U-Boot Rust projects
- Follows Rust `*-sys` naming conventions for FFI binding crates
- Intended to provide a safe, well-documented interfaces to U-Boot functions
- Organized into logical modules with re-exports for convenience

### Dependency Structure
```
examples/rust/demo → u-boot-sys → U-Boot C library
                                      ↓
                              libu-boot.so/.a
```

### Build System Integration
- Uses standard Cargo dependency resolution
- Makefile provides U-Boot-specific environment setup
- Compatible with both U-Boot's build system and standalone Cargo builds

## Features Demonstrated

- **FFI Integration**: Shows how to call U-Boot C functions from Rust
- **Library Initialization**: Proper `ulib_init()` and `ulib_uninit()` usage
- **File Operations**: Using U-Boot's `os_open()`, `os_fgets()`, `os_close()`
- **Print Functions**: Using U-Boot's `ub_printf()` vs Rust's `println!()`
- **Mixed Languages**: Combining Rust and C functionality seamlessly

## Prerequisites

1. **Rust toolchain**: Install from https://rustup.rs/
2. **U-Boot library**: Build U-Boot sandbox with library support:
   ```bash
   make sandbox_defconfig
   make
   ```

## Usage Examples

```bash
# Ensure U-Boot is built first
# Note this builds rust examples at /tmp/b/sandbox/example/rust
make O=/tmp/b/sandbox sandbox_defconfig all

# Build both versions using Makefile
cd examples/rust
make UBOOT_BUILD=/tmp/b/sandbox srctree=../..

# Or build directly with Cargo
env UBOOT_BUILD=/tmp/b/sandbox cargo build --release

# Test dynamic version
LD_LIBRARY_PATH=/tmp/b/sandbox ./demo

# Test static version
./demo-static

# Run tests
make test
```

## License

GPL-2.0+ (same as U-Boot)

Note: Linking with U-Boot's GPL-2.0+ library makes your program subject to
GPL licensing terms.

## Documentation and Further Reading

- See `doc/develop/ulib.rst`
- **C examples**: Located in `examples/ulib/` for comparison
- **u-boot-sys crate**: API documentation via `cargo doc` in `lib/rust/`
