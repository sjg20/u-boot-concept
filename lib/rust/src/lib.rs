// SPDX-License-Identifier: GPL-2.0+
/*
 * U-Boot FFI bindings for Rust
 *
 * This crate provides Rust FFI bindings for U-Boot library functions,
 * equivalent to include/u-boot-lib.h
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

//! # U-Boot Bindings
//!
//! This crate provides Rust FFI bindings for U-Boot library functions.
//! It includes bindings for:
//!
//! - U-Boot library management functions (`ulib_*`)
//! - U-Boot API functions (`ub_*`)
//! - OS abstraction functions (`os_*`)
//!
//! ## Usage
//!
//! ```rust,no_run
//! use uboot_sys::{ulib_init, ulib_uninit, ub_printf};
//! use std::ffi::CString;
//!
//! let program_name = CString::new("my_program").unwrap();
//! unsafe {
//!     if ulib_init(program_name.as_ptr() as *mut _) == 0 {
//!         ub_printf(b"Hello from U-Boot!\n\0".as_ptr() as *const _);
//!         ulib_uninit();
//!     }
//! }
//! ```

#![allow(clippy::manual_c_str_literals)]
#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int};

/// U-Boot library management functions
pub mod uboot_lib {
    use super::*;

    extern "C" {
        /// Set up the U-Boot library
        ///
        /// # Arguments
        ///
        /// * `progname` - Program name to use (must be a writeable string,
        ///                typically argv\[0\])
        ///
        /// # Returns
        ///
        /// 0 if OK, negative error code on error
        pub fn ulib_init(progname: *mut c_char) -> c_int;

        /// Shut down the U-Boot library
        ///
        /// Call this when your program has finished using the library,
        /// before it exits
        pub fn ulib_uninit();

        /// Get the version string
        ///
        /// # Returns
        ///
        /// Full U-Boot version string
        pub fn ulib_get_version() -> *const c_char;
    }
}

/// U-Boot API functions
pub mod uboot_api {
    use super::*;

    extern "C" {
        /// Print directly to console (equivalent to printf)
        ///
        /// # Arguments
        ///
        /// * `fmt` - The format string to use
        /// * `...` - Arguments for the format string
        ///
        /// # Returns
        ///
        /// Number of characters printed
        pub fn ub_printf(fmt: *const c_char, ...) -> c_int;

        /// Format a string and place it in a buffer
        ///
        /// # Arguments
        ///
        /// * `buf` - The buffer to place the result into
        /// * `size` - The size of the buffer, including the trailing null
        ///            space
        /// * `fmt` - The format string to use
        /// * `...` - Arguments for the format string
        ///
        /// # Returns
        ///
        /// The number of characters which would be generated for the given
        /// input, excluding the trailing null, as per ISO C99
        pub fn ub_snprintf(
            buf: *mut c_char,
            size: usize,
            fmt: *const c_char,
            ...
        ) -> c_int;

        /// Format a string and place it in a buffer
        ///
        /// # Arguments
        ///
        /// * `buf` - The buffer to place the result into
        /// * `fmt` - The format string to use
        /// * `...` - Arguments for the format string
        ///
        /// # Returns
        ///
        /// The number of characters written into buf
        pub fn ub_sprintf(buf: *mut c_char, fmt: *const c_char, ...) -> c_int;
    }
}

/// OS abstraction functions
pub mod os {
    use super::*;

    extern "C" {
        /// Access the OS open() system call
        ///
        /// # Arguments
        ///
        /// * `pathname` - Path to the file to open
        /// * `flags` - Open flags
        ///
        /// # Returns
        ///
        /// File descriptor, or negative error code on error
        pub fn os_open(pathname: *const c_char, flags: c_int) -> c_int;

        /// Access the OS close() system call
        ///
        /// # Arguments
        ///
        /// * `fd` - File descriptor to close
        ///
        /// # Returns
        ///
        /// 0 on success, negative error code on error
        pub fn os_close(fd: c_int) -> c_int;

        /// Read a string from a file stream
        ///
        /// # Arguments
        ///
        /// * `s` - Buffer to store the string
        /// * `size` - Maximum number of characters to read
        /// * `fd` - File descriptor to read from
        ///
        /// # Returns
        ///
        /// Pointer to the string on success, null pointer on EOF or
        /// error
        pub fn os_fgets(s: *mut c_char, size: c_int, fd: c_int) -> *mut c_char;
    }
}

// Re-export commonly used functions at crate root
pub use uboot_lib::{ulib_get_version, ulib_init, ulib_uninit};
pub use uboot_api::{ub_printf, ub_snprintf, ub_sprintf};
pub use os::{os_close, os_fgets, os_open};
