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
use std::os::raw::{c_char};
use std::ptr;
use std::result::Result::{Ok, Err};

/// # Safety
///
/// C-style interface for demangling.
#[no_mangle]
pub unsafe extern "C" fn rustc_demangle(mangled: *const c_char) -> *mut c_char
{
    if mangled.is_null() {
        return ptr::null_mut();
    }
    let mangled_str = match std::ffi::CStr::from_ptr(mangled).to_str() {
        Ok(s) => s,
        Err(_) => return ptr::null_mut(),
    };

    match rustc_demangle::try_demangle(mangled_str) {
        Ok(demangle) => {
            let size = demangle.to_string().len();
            let out = unsafe { System.alloc_zeroed(Layout::from_size_align_unchecked(size, 1)) };
            if out.is_null() {
                return ptr::null_mut();
            }

            match write!(unsafe { std::slice::from_raw_parts_mut(out, size) }, "{:#}\0", demangle) {
                Ok(_) => {
                    unsafe { std::slice::from_raw_parts_mut(out, size) }.as_mut_ptr() as *mut c_char
                }
                Err(_) => ptr::null_mut(),
            }
        }
        Err(_) => ptr::null_mut(),
    }
}
