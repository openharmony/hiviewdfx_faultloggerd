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
#include "procinfo.h"
#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {

class DfxCatchFrameLocal {
public:
    DfxCatchFrameLocal();
    explicit DfxCatchFrameLocal(int32_t pid);
    ~DfxCatchFrameLocal();

    /**
     * @brief  initialize catch frame
     *
     * @return if succeed return true, otherwise return false
    */
    bool InitFrameCatcher();

    /**
     * @brief release thread by thread id
     *
     * @param tid  thread id
     * @return if succeed return true, otherwise return false
    */
    bool ReleaseThread(int tid);

    /**
     * @brief catch thread backtrace frame by thread id
     *
     * @param frames  backtrace frames(output parameter)
     * @param skipFrames skip frames number
     * @param releaseThread flag for whether to release thread after completing frames catching
     * @return if succeed return true, otherwise return false
    */
    bool CatchFrame(int tid, std::vector<DfxFrame>& frames, int skipFrames = 0, bool releaseThread = false);

    /**
     * @brief catch thread backtrace frame by thread id
     *
     * @param mapFrames  namespace tid and backtrace frames(output parameter)
     * @param releaseThread flag for whether to release thread after completing frames catching
     * @return if succeed return true, otherwise return false
    */
    bool CatchFrame(std::map<int, std::vector<DfxFrame>>& mapFrames, bool releaseThread = false);

    /**
     * @brief release the resource of frame catcher
     *
    */
    void DestroyFrameCatcher();

private:
    bool CatchFrameCurrTid(std::vector<DfxFrame>& frames, int skipFrames = 0, bool releaseThread = false);
    bool CatchFrameLocalTid(int tid, std::vector<DfxFrame>& frames, int skipFrames = 0, bool releaseThread = false);

private:
    std::mutex mutex_;
    int32_t pid_;
    struct ProcInfo procInfo_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
