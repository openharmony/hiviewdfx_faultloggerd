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

#ifndef KERNEL_SNAPSHOT_DATA_H
#define KERNEL_SNAPSHOT_DATA_H

#include <string>
#include <unordered_map>

namespace OHOS {
namespace HiviewDFX {
constexpr int DECIMAL_BASE = 10;

enum class CrashSection {
    BUILD_INFO,
    TIME_STAMP,
    PID,
    UID,
    PROCESS_NAME,
    REASON,
    FAULT_THREAD_INFO,
    CREGISTERS,
    MEMORY_NEAR_REGISTERS,
    FAULT_STACK,
    MAPS,
    EXCEPTION_REGISTERS,
    INVALID_SECTION
};

enum class SnapshotSection {
    TRANSACTION_START,
    EXCEPTION_REGISTERS,
    ABORT_ADDRESS_PTE,
    THREAD_INFO,
    DUMP_REGISTERS,
    DUMP_FPU_OR_SIMD_REGISTERS,
    STACK_BACKTRACE,
    ELF_LOAD_INFO,
    DATA_ON_TARGET_OF_LAST_BL,
    DATA_AROUND_REGS,
    CONTENT_OF_USER_STACK,
    BASE_ACTV_DUMPED,
    PROCESS_STATISTICS,
    TRANSACTION_END
};

struct SnapshotSectionKeyword {
    SnapshotSection type;
    const char* key;
};

const SnapshotSectionKeyword SNAPSHOT_SECTION_KEYWORDS[] = {
    {SnapshotSection::TRANSACTION_START,              "[transaction start] now mono_time"},
    {SnapshotSection::EXCEPTION_REGISTERS,            "Exception registers:"},
    {SnapshotSection::ABORT_ADDRESS_PTE,              "Abort address pte"},
    {SnapshotSection::THREAD_INFO,                    "Thread info:"},
    {SnapshotSection::DUMP_REGISTERS,                 "Dump registers:"},
    {SnapshotSection::DUMP_FPU_OR_SIMD_REGISTERS,     "Dump fpu or simd registers:"},
    {SnapshotSection::STACK_BACKTRACE,                "Stack backtrace"},
    {SnapshotSection::ELF_LOAD_INFO,                  "Elf load info"},
    {SnapshotSection::DATA_ON_TARGET_OF_LAST_BL,      "Data on target of last"},
    {SnapshotSection::DATA_AROUND_REGS,               "Data around regs"},
    {SnapshotSection::CONTENT_OF_USER_STACK,          "Contents of user stack"},
    {SnapshotSection::BASE_ACTV_DUMPED,               "[base actv dumped]"},
    {SnapshotSection::PROCESS_STATISTICS,             "Process statistics:"},
    {SnapshotSection::TRANSACTION_END,                "[transaction end] now mono_time"}
};

using CrashMap = std::unordered_map<CrashSection, std::string>;
} // namespace HiviewDFX
} // namespace OHOS
#endif
