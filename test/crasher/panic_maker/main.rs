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

//! Panic trigger for Rust.

extern crate panic_handler;
extern crate stacktrace_rust;

use std::{panic, thread};
use hilog_rust::{hilog, HiLogLabel, LogType};
use std::ffi::{c_char, CString};

const LOG_LABEL: HiLogLabel = HiLogLabel {
    log_type: LogType::LogCore,
    domain: 0xd003200,
    tag: "testTag",
};

/// function main
fn main() {
    panic_handler::init();
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 2 {
        println!("Invalid arguments.");
        usage(&args);
        return;
    }

    test_panic(&args);
}

fn usage(args: &[String]) {
    println!("Usage:");
    println!("{} cmd [option]", args[0]);
    println!("cmd:");
    println!("\tmain\t\tConstruct a panic in the main thread.");
    println!("\t\t\tSpecifies the number of hilog lines using options.");
    println!("\tchild\t\tConstruct a panic in the child thread.");
    println!("\t\t\tSpecifies the number of hilog lines using options.");
    println!("\tmulti\t\tMulti-thread test C language interface GetTrace.");
    println!("\tprint_trace\tTest the PrintTrace interface in C language.");
    println!("\tget_trace\tTest the GetTrace interface in C language.");
}

fn test_panic(args: &[String]) {
    let read_cnt = |args: &[String]| -> Option<u32> {
        match args.len() {
            3 => Some(args[2].parse().unwrap()),
            4 .. => {
                usage(args);
                None
            },
            _ => Some(0),
        }
    };
    match args[1].as_str() {
        "main" => {
            let Some(hilog_cnt) = read_cnt(args) else {
                return;
            };
            for cnt in 1 ..= hilog_cnt {
                hilog_rust::info!(LOG_LABEL, "hilog print test. No.{cnt}");
            }
            panic_main();
        },
        "child" => {
            let Some(hilog_cnt) = read_cnt(args) else {
                return;
            };
            for cnt in 1 ..= hilog_cnt {
                hilog_rust::info!(LOG_LABEL, "hilog print test. No.{cnt}");
            }
            panic_child();
        },
        "multi" => get_trace_in_multi_thread(),
        "print_trace" => {
            stacktrace_rust::print_trace(1);
        },
        "get_trace" => {
            let ret = stacktrace_rust::get_trace(false);
            println!("{}", ret);
        },
        _ => usage(args),
    }
}

fn panic_main() {
    panic!("panic in main thread");
}

fn panic_child() {
    let ret = thread::spawn(move || {
        panic!("panic in child thread");
    }).join();
    println!("{:?}", ret);
}

fn get_trace_in_multi_thread() {
    let mut handles = vec![];
    for _ in 0..50 {
        let handle = thread::spawn(move || {
            let trace = stacktrace_rust::get_trace(false);
            println!("{}", trace);
        });
        handles.push(handle);
    }
    for handle in handles {
        handle.join().unwrap();
    }
}
