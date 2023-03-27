/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <libunwind.h>

#include "backtrace.h"
#include "dfx_symbols_cache.h"

namespace OHOS {
namespace HiviewDFX {
class BacktraceLocalThread {
public:
    explicit BacktraceLocalThread(int32_t tid);
    ~BacktraceLocalThread();

    bool Unwind(unw_addr_space_t as, std::shared_ptr<DfxSymbolsCache> cache, size_t skipFrameNum,
        bool fast = false, bool releaseThread = true);
    void ReleaseThread();

    std::string GetFramesStr();
    const std::vector<NativeFrame>& GetFrames() const;

    static std::string GetNativeFrameStr(const NativeFrame& frame);
    static bool GetBacktraceFrames(std::vector<NativeFrame>& frames, int32_t tid, size_t skipFrameNum, bool fast);
    static bool GetBacktraceString(std::string& out, int32_t tid, size_t skipFrameNum, bool fast);
private:
    bool UnwindCurrentThread(unw_addr_space_t as, std::shared_ptr<DfxSymbolsCache> cache, 
        size_t skipFrameNum, bool fast = false);

private:
    int32_t tid_;
    std::vector<NativeFrame> frames_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // UNWIND_LOCAL_THREAD_H
