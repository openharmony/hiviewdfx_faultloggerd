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
#ifndef DFX_COREDUMP_COMMON_H
#define DFX_COREDUMP_COMMON_H
#if defined(__aarch64__)

#include <cstdint>
#include <sys/types.h>

namespace OHOS {
namespace HiviewDFX {

struct DumpMemoryRegions {
    uint64_t startHex;
    uint64_t endHex;
    uint64_t offsetHex;
    uint64_t memorySizeHex;
    char start[17];
    char end[17];
    char priority[5];
    char offset[9];
    char dev[16];
    unsigned int inode;
    char pathName[256];
};

struct FileHeader {
    uint64_t count;
    uint64_t pageSize;
};

struct FileMap {
    uint64_t start;
    uint64_t end;
    uint64_t offset;
};

struct UserPacMask {
    uint64_t dataMask;
    uint64_t insnMask;
};

struct CoreDumpThread {
    pid_t targetPid = 0;
    pid_t targetTid = 0;
    pid_t vmPid = 0;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
#endif