/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

//! stacktrace tools for Rust.

use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_uint};
use std::string::String;
use std::sync::Mutex;

static TRACE_MUTEX : Mutex<i32> = Mutex::new(0);

#[link(name = "backtrace_local")]
extern "C" {
    fn PrintTrace(fd : c_int) -> bool;
    fn GetTrace(skip_frame_num : c_uint, max_frame_num : c_uint) -> *const c_char;
}

/// Print Rust trace into File
pub fn print_trace(fd : i32) -> bool {
    unsafe {
        PrintTrace(fd)
    }
}

/// Get Rust trace by returned parameter
#[allow(unused_variables)]
pub fn get_trace(is_crash : bool) -> String {
    unsafe {
        let mutex = TRACE_MUTEX.lock().unwrap();
        let mut skip_frame_num = 0;
        let max_frame_num = 256;
        if is_crash {
            skip_frame_num = 8;
        }
        let trace = GetTrace(skip_frame_num, max_frame_num);
        CStr::from_ptr(trace).to_str().unwrap().to_owned()
    }
}