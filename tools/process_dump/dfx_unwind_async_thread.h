/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef DFX_UNWIND_ASYNC_THREAD_H
#define DFX_UNWIND_ASYNC_THREAD_H

#include <csignal>
#include <string>
#include "dfx_thread.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
class DfxUnwindAsyncThread {
public:
    DfxUnwindAsyncThread(DfxThread& thread, Unwinder& unwinder, uint64_t stackId)
        : thread_(thread), unwinder_(unwinder), stackId_(stackId) {}
    bool UnwindStack(pid_t vmPid = 0);
    std::string unwindFailTip = "";
private:
    void GetSubmitterStack(std::vector<DfxFrame> &submitterFrames);
#ifndef __x86_64__
    void CreateFrame(DfxMaps& dfxMaps, size_t index, uintptr_t pc, uintptr_t sp = 0);
    void UnwindThreadFallback();
#endif
    void UnwindThreadByParseStackIfNeed();
    DfxThread& thread_;
    Unwinder& unwinder_;
    uint64_t stackId_ = 0;
};
}
}
#endif