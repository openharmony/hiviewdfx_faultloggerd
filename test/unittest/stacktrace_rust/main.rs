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

extern crate stacktrace_rust;

use std::fs;
use std::fs::File;
use std::os::fd::AsRawFd;
use std::path::Path;
use std::thread;

#[test]
fn test_get_trace() {
    let trace = stacktrace_rust::get_trace();
    assert!(!trace.is_empty());
    assert!(trace.contains("#00"));
    assert!(trace.contains("libstacktrace_rust.dylib.so"));
    assert!(trace.contains("test_stacktrace_rust"));
}

#[test]
fn test_get_trace_in_multithread() {
    let mut handles = vec![];
    for _ in 0..50 {
        let handle = thread::spawn(move || {
            let trace = stacktrace_rust::get_trace();
            assert!(!trace.is_empty());
            assert!(trace.contains("#00"));
            assert!(trace.contains("libstacktrace_rust.dylib.so"));
            assert!(trace.contains("test_stacktrace_rust"));
        });
        handles.push(handle);
    }
    for handle in handles {
        handle.join().unwrap();
    }
}

#[test]
fn test_print_trace() {
    let path = Path::new("/data/stacktrace_test_file001");
    let display = path.display();
    let output = match File::create(path) {
        Err(why) => panic!("couldn't create {}: {:?}", display, why),
        Ok(output) => output,
    };
    assert!(stacktrace_rust::print_trace(output.as_raw_fd()));
    let trace = fs::read_to_string(path).unwrap();
    assert!(!trace.is_empty());
    assert!(trace.contains("#00"));
    assert!(trace.contains("libstacktrace_rust.dylib.so"));
    assert!(trace.contains("test_stacktrace_rust"));
}