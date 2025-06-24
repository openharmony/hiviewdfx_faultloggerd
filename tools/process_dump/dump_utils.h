/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#ifndef DUMP_UTILS_H
#define DUMP_UTILS_H
#include "unwinder.h"
#include "dfx_dump_request.h"
#include "dfx_thread.h"
#include <cinttypes>
#include <memory>
namespace OHOS {
namespace HiviewDFX {
class DumpUtils {
public:
    static bool ParseLockInfo(Unwinder& unwinder, int32_t vmPid, int32_t tid);
    static bool IsLastValidFrame(const DfxFrame& frame);
    static void GetThreadKernelStack(DfxThread& thread);
    static std::string ReadStringByPtrace(pid_t tid, uintptr_t startAddr, size_t maxLen);
    static std::string GetStackTrace(const std::vector<DfxFrame>& frames);
    static bool ReadTargetMemory(pid_t tid, uintptr_t addr, uintptr_t &value);
    static std::string GetSelfBundleName();
    static void InfoCrashUnwindResult(const ProcessDumpRequest& request, bool isUnwindSucc);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
