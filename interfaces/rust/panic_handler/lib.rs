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
#![feature(rustc_private)]
extern crate hisysevent;
extern crate libc;
extern crate stacktrace_rust;
use std::ffi::CString;
use std::fs;
use std::os::raw::{c_char};
use std::panic;
use std::panic::PanicInfo;
use std::process;
use std::time::{UNIX_EPOCH, SystemTime};
use hilog_rust::{error, hilog, HiLogLabel, LogType};
use hisysevent::{EventType};

const LOG_LABEL: HiLogLabel = HiLogLabel {
    log_type: LogType::LogCore,
    domain: 0xD002D11,
    tag: "DfxFaultLogger"
};

/// panic handler
fn panic_handler() {
    panic::set_hook(Box::new(move |info| {
        report_info_handler(info)
    }));
}

fn read_process_name(pid: u32) -> String {
    let path = format!("/proc/{}/cmdline", pid);
    let binding = fs::read_to_string(path).unwrap();
    let mut process_name = binding.as_str();
    if process_name.find('\0').is_some() {
        (process_name, _) = process_name.split_at(process_name.find('\0').unwrap());
    }
    if process_name.rfind('/').is_some() {
        (_, process_name) = process_name.split_at(process_name.rfind('/').unwrap() + 1);
    }
    process_name.to_string()
}

fn read_thread_name(pid: u32) -> String {
    let path = format!("/proc/{}/comm", pid);
    fs::read_to_string(path).unwrap()
}

#[allow(unused_variables)]
fn report_info_handler(info: &PanicInfo) {
    let (file, line) = info.location().map(|loc| (loc.file(), loc.line())).unwrap_or(("<unknown>", 0));
    let errmsg = match info.payload().downcast_ref::<&'static str>() {
        Some(s) => *s,
        None => match info.payload().downcast_ref::<String>() {
            Some(s) => &**s,
            None => "Box<Any>",
        },
    };

    let pid = process::id();
    let process_name = read_process_name(pid);
    let thread_name = read_thread_name(pid);
    let panic_reason = format!("file:{} line:{} message:{}", file, line, errmsg);
    let thread_label = format!("Thread name:{}{}", thread_name, stacktrace_rust::get_trace(true).as_str());
    let happen_time = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis();
    let ret = hisysevent::write(
        "RELIABILITY",
        "RUST_PANIC",
        EventType::Fault,
        &[hisysevent::build_str_param!("MODULE", process_name.as_str()),
            hisysevent::build_str_param!("REASON", panic_reason.as_str()),
            hisysevent::build_number_param!("PID", pid),
            hisysevent::build_number_param!("TID", unsafe { libc::gettid() }),
            hisysevent::build_number_param!("UID", unsafe { libc::getuid() }),
            hisysevent::build_str_param!("SUMMARY", thread_label.as_str()),
            hisysevent::build_number_param!("HAPPEN_TIME", happen_time)
        ]
    );
    if ret != 0 {
        error!(LOG_LABEL, "RUST_PANIC hisysevent write failed");
    }
}

/// Initializes the panic hook
pub fn init() {
    panic_handler();
}