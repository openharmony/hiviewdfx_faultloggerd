/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#ifndef UNWIND_LOCAL_THREAD_H
#define UNWIND_LOCAL_THREAD_H

#include <cinttypes>
#include <memory>
#include <vector>

#include "dfx_frame.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
constexpr int32_t BACKTRACE_CURRENT_THREAD = -1;

class BacktraceLocalThread {
public:
    explicit BacktraceLocalThread(int32_t tid, std::shared_ptr<Unwinder> unwinder);
    ~BacktraceLocalThread();

    bool Unwind(bool fast = false, size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);

    const std::vector<DfxFrame>& GetFrames() const;
    std::string GetFormattedStr(bool withThreadName = false, bool isJson = false);

private:
    int32_t tid_;
    std::vector<DfxFrame> frames_;
    std::shared_ptr<Unwinder> unwinder_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // UNWIND_LOCAL_THREAD_H
