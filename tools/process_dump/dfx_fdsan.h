/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
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

#ifndef DFX_FDSAN_H
#define DFX_FDSAN_H

#define FD_TABLE_SIZE 128

struct FdEntry {
    _Atomic(uint64_t) close_tag;
    _Atomic(char) signal_flag;
};

struct FdTableOverflow {
    size_t len;
    struct FdEntry entries[];
};

struct FdTable {
    _Atomic(enum fdsan_error_level) error_level;
    struct FdEntry entries[FD_TABLE_SIZE];
    _Atomic(struct FdTableOverflow*) overflow;
};

struct FDInfo {
    std::string path;
    uint64_t fdsanOwner;
};

#endif
