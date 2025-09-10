// SPDX-License-Identifier: GPL-2.0+
/*
 * Helper functions for Rust U-Boot library demo
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#![allow(clippy::manual_c_str_literals)]

use std::os::raw::c_char;

use u_boot_sys::ub_printf;

/// Show the demo banner
pub fn demo_show_banner() {
    unsafe {
        ub_printf(
            b"U-Boot Library Demo Helper\n\0".as_ptr() as *const c_char,
        );
        ub_printf(
            b"==========================\n\0".as_ptr() as *const c_char,
        );
    }
}

/// Show the demo footer
pub fn demo_show_footer() {
    unsafe {
        ub_printf(
            b"=================================\n\0".as_ptr() as *const c_char,
        );
        ub_printf(
            b"Demo complete (hi from rust)\n\0".as_ptr() as *const c_char,
        );
    }
}

/// Add two numbers and print the result
///
/// # Arguments
///
/// * `a` - First number
/// * `b` - Second number
///
/// # Returns
///
/// Sum of the two numbers
pub fn demo_add_numbers(a: i32, b: i32) -> i32 {
    unsafe {
        ub_printf(
            b"helper: Adding %d + %d = %d\n\0".as_ptr() as *const c_char,
            a,
            b,
            a + b,
        );
    }
    a + b
}