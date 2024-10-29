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

#include <cinttypes>
#include <csignal>
#include <map>
#include <memory>
#include <string>
#include "dfx_thread.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
class DfxUnwindAsyncThread {
public:
    DfxUnwindAsyncThread(std::shared_ptr<DfxThread> thread, std::shared_ptr<Unwinder> unwinder, uint64_t stackId)
        : thread_(thread), unwinder_(unwinder), stackId_(stackId) {}
    bool UnwindStack(pid_t vmPid = 0);
    std::string tip = "";
private:
    void GetSubmitterStack(std::vector<DfxFrame> &submitterFrames);
    void UnwindThreadFallback();
    void UnwindThreadByParseStackIfNeed();
    void MergeStack(std::vector<DfxFrame> &submitterFrames);
    std::shared_ptr<DfxThread> thread_ = nullptr;
    std::shared_ptr<Unwinder> unwinder_ = nullptr;
    uint64_t stackId_ = 0;
};
}
}
#endif