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

//! panic handler for Rust.

use std::ffi::CString;
use std::os::raw::{c_uint, c_char};
use std::panic;
use std::panic::PanicInfo;

/// trace info struct
#[repr(C)]
#[derive(Debug)]
pub struct RustPanicInfo {
    /// filename
    pub filename: *const c_char,
    /// line
    pub line: c_uint,
    /// msg
    pub msg: *const c_char,
}

#[link(name = "panic_reporter.z")]
extern "C" {
    fn ReportTraceInfo(info: *mut RustPanicInfo) -> bool;
    fn PrintTraceInfo() -> bool;
}

/// backtrace mode
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum BacktraceMode {
    /// Print.
    PrintInfo,
    /// Report.
    ReportInfo,
}

/// Configures the panic hook.
#[derive(Debug)]
pub struct HookBuilder {
    backtrace_mode: BacktraceMode,
}

impl HookBuilder {
    /// set backtrace mode
    pub fn set_backtrace_mode(mut self, mode: BacktraceMode) -> Self {
        self.backtrace_mode = mode;
        self
    }

    /// panic handler
    pub fn panic_handler(self) {
        match self.backtrace_mode {
            BacktraceMode::PrintInfo => panic::set_hook(Box::new(move |info| self.print_info_handler(info))),
            BacktraceMode::ReportInfo => panic::set_hook(Box::new(move |info| self.report_info_handler(info))),
        };
    }

    #[allow(unused_variables)]
    fn print_info_handler(&self, info: &PanicInfo) {
        unsafe {
            PrintTraceInfo();
        }
    }

    #[allow(unused_variables)]
    fn report_info_handler(&self, info: &PanicInfo) {
        let (file, line) = info.location().map(|loc| (loc.file(), loc.line())).unwrap_or(("<unknown>", 0));
        let errmsg = match info.payload().downcast_ref::<&'static str>() {
            Some(s) => *s,
            None => match info.payload().downcast_ref::<String>() {
                Some(s) => &**s,
                None => "Box<Any>",
            },
        };

        let filename = CString::new(file).unwrap().into_raw();
        let msg = CString::new(errmsg).unwrap().into_raw();
        let info : RustPanicInfo = RustPanicInfo {filename, line, msg};
        let panic_info = Box::into_raw(Box::new(info));
        unsafe {
            ReportTraceInfo(panic_info);
        }
    }
}

/// Default the builder.
impl Default for HookBuilder {
    fn default() -> Self {
        Self {
            backtrace_mode: BacktraceMode::ReportInfo,
        }
    }
}

/// Initializes the panic hook
pub fn init() {
    HookBuilder::default().panic_handler();
}