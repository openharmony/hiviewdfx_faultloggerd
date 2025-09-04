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
#ifndef FAULT_COREDUMP_COMMON_H
#define FAULT_COREDUMP_COMMON_H

#include <csignal>
#include <cstdint>

#include "dfx_regs.h"

namespace OHOS {
namespace HiviewDFX {

constexpr size_t ADDR_STR_LEN = 17;
constexpr size_t PERM_STR_LEN = 5;
constexpr size_t OFFSET_STR_LEN = 9;
constexpr size_t DEV_STR_LEN = 16;
constexpr size_t PATH_MAX_LEN = 256;

struct DumpMemoryRegions {
    uint64_t startHex;
    uint64_t endHex;
    uint64_t offsetHex;
    uint64_t memorySizeHex;
    char start[ADDR_STR_LEN];
    char end[ADDR_STR_LEN];
    char priority[PERM_STR_LEN];
    char offset[OFFSET_STR_LEN];
    char dev[DEV_STR_LEN];
    unsigned int inode;
    char pathName[PATH_MAX_LEN];
};

struct FileHeader {
    uint64_t count{0};
    uint64_t pageSize {0};
};

struct FileMap {
    uint64_t start{0};
    uint64_t end{0};
    uint64_t offset{0};
};

struct UserPacMask {
    uint64_t dataMask{0};
    uint64_t insnMask{0};
};

struct CoredumpProc {
    pid_t targetPid {0};
    pid_t keyTid {0};

    void Init(pid_t pid, pid_t tid)
    {
        targetPid = pid;
        keyTid = tid;
    }

    bool IsValid() const
    {
        return targetPid > 0 && keyTid > 0;
    }
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
