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

#include "backtrace_local.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include <hilog/log.h>

#include "backtrace_local_thread.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktrace"
#define LOG_DOMAIN 0xD002D11

constexpr int SKIP_ONE_FRAME = 1; // skip current frame
}

bool PrintBacktrace(int32_t fd, bool fast)
{
    std::vector<NativeFrame> frames;
    bool ret = BacktraceLocalThread::GetBacktraceFrames(frames, BACKTRACE_CURRENT_THREAD, SKIP_ONE_FRAME, fast);
    if (!ret) {
        return false;
    }

    for (auto const& frame : frames) {
        auto line = BacktraceLocalThread::GetNativeFrameStr(frame);
        if (fd < 0) {
            // print to hilog
            HILOG_INFO(LOG_CORE, " %{public}s", line.c_str());
        } else {
            dprintf(fd, "    %s", line.c_str());
        }
    }
    return ret;
}

bool GetBacktrace(std::string& out, bool fast)
{
    return BacktraceLocalThread::GetBacktraceString(out, BACKTRACE_CURRENT_THREAD, SKIP_ONE_FRAME, fast);
}
} // namespace HiviewDFX
} // namespace OHOS
