/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SAFE_READERS_H
#define SAFE_READERS_H

#include <cstdio>
#include <stdint.h>

#include "dfx_define.h"

class SafeReaders {
public:
    SafeReaders();
    ~SafeReaders();
    SafeReaders(const SafeReaders&) = delete;
    SafeReaders& operator=(const SafeReaders&) = delete;

    NO_SANITIZE ssize_t CopyReadbaleBufSafe(uintptr_t destPtr, size_t destLen, uintptr_t srcPtr, size_t srcLen);

private:
    NO_SANITIZE bool IsReadbaleAddr(uintptr_t addr);
    static uintptr_t GetCurrentPageEndAddr(uintptr_t addr);

    int pfd_[PIPE_NUM_SZ] = {-1, -1};
};

#endif // READ_SAFE_H
