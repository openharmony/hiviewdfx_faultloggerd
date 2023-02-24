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

use std::{panic, thread};

/// function main
fn main() {
    let args: Vec<String> = std::env::args().collect();
    panic_handler::init();
    if args.len() > 1 {
        test_panic(&args[1]);
    } else {
        println!("Invalid arguments.");
    }
}

fn test_panic(pt: &String) {
    if pt == "main" {
        panic_main();
    } else if pt == "child" {
        panic_child();
    }
}

fn panic_main() {
    panic!("panic in main thread");
}

fn panic_child() {
    let r = thread::spawn(move || {
        panic!("panic in child thread");
    }).join();
    println!("{:?}", r);
}
