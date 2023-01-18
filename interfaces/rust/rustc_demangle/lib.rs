/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! rustc demangle for Rust.

#![feature(rustc_private)]
extern crate rustc_demangle;

use std::alloc::{GlobalAlloc, Layout, System};
use std::io::Write;
use std::os::raw::{c_char, c_int};
use std::{ptr, result};

type Result<T> = result::Result<T, Status>;

enum Status {
    AllocFailure = -1,
    DemangleFailure = -2,
    InvalidArgs = -3,
}

/// Convenience function to set return status if a location was provided.
unsafe fn set_status(status: *mut c_int, val: c_int) {
    if !status.is_null() {
        *status = val;
    }
}

/// Region from the system allocator for demangler output. 
/// We use the system allocator because the intended client is C/C++ code 
/// which may not be using the Rust allocator.
struct SystemBuffer {
    buf: *mut u8,
    size: usize,
    size_out: *mut usize,
}

impl SystemBuffer {
    const DEFAULT_BUFFER_SIZE: usize = 1024;
    fn new(size: usize) -> Result<Self> {
        let buf = unsafe { System.alloc_zeroed(Layout::from_size_align_unchecked(size, 1)) };
        if buf.is_null() {
            Err(Status::AllocFailure)
        } else {
            Ok(Self {
                buf,
                size,
                size_out: ptr::null_mut(),
            })
        }
    }
    /// Safety: If buf is non-null, size must be non-null 
    /// and point to the non-zero size of the buffer provided in buf.
    /// Takes ownership of the buffer passed in (and may reallocate it).
    /// size must outlive the resulting buffer if non-null.
    unsafe fn from_raw(buf: *mut c_char, size: *mut usize) -> Result<Self> {
        if buf.is_null() {
            if !size.is_null() {
                *size = Self::DEFAULT_BUFFER_SIZE;
            }
            let fresh = Self::new(Self::DEFAULT_BUFFER_SIZE)?;
            Ok(Self {
                size_out: size,
                ..fresh
            })
        } else {
            Ok(Self {
                buf: buf as *mut u8,
                size: *size,
                size_out: size,
            })
        }
    }
    fn as_mut_slice(&mut self) -> &mut [u8] {
        unsafe { std::slice::from_raw_parts_mut(self.buf, self.size) }
    }
    fn resize(&mut self) -> Result<()> {
        let new_size = self.size * 2;
        let new_buf = unsafe {
            System.realloc(
                self.buf,
                Layout::from_size_align_unchecked(self.size, 1),
                new_size,
            )
        };
        if new_buf.is_null() {
            Err(Status::AllocFailure)
        } else {
            self.buf = new_buf;
            self.size = new_size;
            if !self.size_out.is_null() {
                unsafe {
                    *self.size_out = new_size;
                }
            }
            Ok(())
        }
    }
}

/// # Safety
///
/// C-style interface for demangling.
/// Demangles symbol given in `mangled` argument into `out` buffer.
///
/// This interface is a drop-in replacement for `__cxa_demangle`, but for Rust demangling.
///
/// If `out` is null, a buffer will be allocated using the system allocator to contain the results.
/// If `out` is non-null, `out_size` must be a pointer to the current size of the buffer, 
/// and `out` must come from the system allocator.
/// If `out_size` is non-null, the size of the output buffer will be written to it.
///
/// If `status` is non-null, it will be set to one of the following values:
/// * 0: Demangling succeeded
/// * -1: Allocation failure
/// * -2: Name did not demangle
/// * -3: Invalid arguments
///
/// Returns null if `mangled` is not Rust symbol or demangling failed.
/// Returns the buffer containing the demangled symbol name otherwise.
///
/// Unsafe as it handles buffers by raw pointers.
///
/// For non-null `out`, `out_size` represents a slight deviation from the `__cxa_demangle` behavior. 
/// For `__cxa_demangle`, the buffer must be at *least* the provided size. 
/// For `rustc_demangle`, it must be the exact buffer size 
/// because it is used in the reconstruction of the `Layout` for use with `::realloc`.
#[no_mangle]
pub unsafe extern "C" fn rustc_demangle(
    mangled: *const c_char,
    out: *mut c_char,
    out_size: *mut usize,
    status: *mut c_int,
) -> *mut c_char {
    match rustc_demangle_native(mangled, out, out_size) {
        Ok(demangled) => {
            set_status(status, 0);
            demangled
        }
        Err(e) => {
            set_status(status, e as c_int);
            ptr::null_mut()
        }
    }
}

unsafe fn rustc_demangle_native(
    mangled: *const c_char,
    out: *mut c_char,
    out_size: *mut usize,
) -> Result<*mut c_char> {
    if mangled.is_null() {
        return Err(Status::InvalidArgs);
    }
    let mangled_str = match std::ffi::CStr::from_ptr(mangled).to_str() {
        Ok(s) => s,
        Err(_) => return Err(Status::InvalidArgs),
    };

    if !out.is_null() {
        if out_size.is_null() {
            return Err(Status::InvalidArgs);
        }
        if *out_size == 0 {
            return Err(Status::InvalidArgs);
        }
    }

    let mut out_buf = SystemBuffer::from_raw(out, out_size)?;

    match rustc_demangle::try_demangle(mangled_str) {
        Ok(demangle) => {
            while write!(out_buf.as_mut_slice(), "{:#}\0", demangle).is_err() {
                out_buf.resize()?;
            }
            Ok(out_buf.as_mut_slice().as_mut_ptr() as *mut c_char)
        }
        Err(_) => Err(Status::DemangleFailure),
    }
}