/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#ifndef DFX_CATCH_FRAME_H
#define DFX_CATCH_FRAME_H

#include <cinttypes>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_symbols.h"

// forward declaration
struct unw_addr_space;
typedef struct unw_addr_space *unw_addr_space_t;

namespace OHOS {
namespace HiviewDFX {

class DfxCatchFrameLocal {
public:
    DfxCatchFrameLocal();
    explicit DfxCatchFrameLocal(int32_t pid);
    ~DfxCatchFrameLocal();

    bool InitFrameCatcher();
    bool ReleaseThread(int tid);
    bool CatchFrame(int tid, std::vector<DfxFrame>& frames, bool releaseThread = false);
    bool CatchFrame(std::map<int, std::vector<DfxFrame>>& mapFrames, bool releaseThread = false);
    void DestroyFrameCatcher();

private:
    bool CatchFrameCurrTid(std::vector<DfxFrame>& frames, bool releaseThread = false);
    bool CatchFrameLocalTid(int tid, std::vector<DfxFrame>& frames, bool releaseThread = false);

private:
    std::mutex mutex_;
    int32_t pid_;
    struct ProcInfo procInfo_;
    unw_addr_space_t as_ {nullptr};
    std::shared_ptr<DfxSymbols> symbol_ {nullptr};
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
