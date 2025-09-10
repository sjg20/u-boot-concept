// SPDX-License-Identifier: GPL-2.0+
/*
 * Rust demo program showing U-Boot library functionality
 *
 * This demonstrates using U-Boot library functions from Rust,
 * modeled after examples/ulib/demo.c
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#![allow(clippy::manual_c_str_literals)]

use std::ffi::CString;
use std::os::raw::{c_char, c_int};

mod rust_helper;

use rust_helper::{demo_add_numbers, demo_show_banner, demo_show_footer};
use u_boot_sys::{os_close, os_fgets, os_open, ub_printf, ulib_get_version,
                 ulib_init, ulib_uninit};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Get program name for ulib_init
    let args: Vec<String> = std::env::args().collect();
    let program_name = CString::new(args[0].clone())?;

    // Init U-Boot library
    let ret = unsafe {
        ulib_init(program_name.as_ptr() as *mut c_char)
    };
    if ret != 0 {
        eprintln!("Failed to initialize U-Boot library");
        return Err("ulib_init failed".into());
    }

    demo_show_banner();

    // Display U-Boot version using ulib_get_version()
    let version_ptr = unsafe { ulib_get_version() };
    unsafe {
        ub_printf(
            b"U-Boot version: %s\n\0".as_ptr() as *const c_char,
            version_ptr,
        );
        ub_printf(b"\n\0".as_ptr() as *const c_char);
    }

    // Use U-Boot's os_open() to open a file
    let filename = CString::new("/proc/version")?;
    let fd = unsafe { os_open(filename.as_ptr(), 0) };
    if fd < 0 {
        eprintln!("Failed to open /proc/version");
        unsafe { ulib_uninit(); }
        return Err("os_open failed".into());
    }

    unsafe {
        ub_printf(
            b"System version:\n\0".as_ptr() as *const c_char,
        );
    }

    // Read lines using U-Boot's os_fgets()
    let mut lines = 0;
    let mut buffer = [0i8; 256]; // Use array instead of Vec to avoid heap
                                  // allocation
    loop {
        let result = unsafe {
            os_fgets(buffer.as_mut_ptr(), buffer.len() as c_int, fd)
        };
        if result.is_null() {
            break;
        }
        
        unsafe {
            ub_printf(
                b"  %s\0".as_ptr() as *const c_char,
                buffer.as_ptr(),
            );
        }
        lines += 1;
    }

    unsafe { os_close(fd) };

    unsafe {
        ub_printf(
            b"\nRead %d line(s) using U-Boot library functions.\n\0"
                .as_ptr() as *const c_char,
            lines,
        );
    }

    // Test the helper function
    let result = demo_add_numbers(42, 13);
    unsafe {
        ub_printf(
            b"Helper function result: %d\n\0".as_ptr() as *const c_char,
            result,
        );
    }

    demo_show_footer();

    // Clean up
    unsafe {
        ulib_uninit();
    }

    Ok(())
}
