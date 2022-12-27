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
    BacktraceLocalThread(int32_t tid);
    ~BacktraceLocalThread() = default;

    bool Unwind(unw_addr_space_t as, std::unique_ptr<DfxSymbolsCache>& cache, size_t skipFrameNum);
    const std::vector<NativeFrame>& GetFrames() const;

private:
    bool GetUnwindContext(unw_context_t& context);
    void DoUnwind(unw_addr_space_t as, unw_context_t& context, std::unique_ptr<DfxSymbolsCache>& cache,
        size_t skipFrameNum);
    void DoUnwindCurrent(unw_addr_space_t as, std::unique_ptr<DfxSymbolsCache>& cache, size_t skipFrameNum);

private:
    int32_t tid_;
    std::vector<NativeFrame> frames_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // UNWIND_LOCAL_THREAD_H
