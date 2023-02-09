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

#include "backtrace_utils.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include <hilog/log.h>

#include "dfx_dump_catcher.h"
#include "backtrace_local_thread.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktrace"
#define LOG_DOMAIN 0xD002D11
bool GetBacktraceFrames(std::vector<NativeFrame>& frames)
{
    auto catcher = std::make_shared<OHOS::HiviewDFX::DfxDumpCatcher>();
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (!catcher->InitFrameCatcher()) {
        return false;
    }

    if (catcher->CatchFrame(frames) == false) {
        return false;
    }

    catcher->DestroyFrameCatcher();
    return true;
}

void PrintStr(int32_t fd, const std::string& line)
{
    if (fd < 0) {
        // print to hilog
        HILOG_INFO(LOG_CORE, " %{public}s", line.c_str());
        return;
    }

    dprintf(fd, "    %s", line.c_str());
}
}

bool PrintBacktrace(int32_t fd)
{
    std::vector<NativeFrame> frames;
    if (!GetBacktraceFrames(frames)) {
        return false;
    }

    for (auto const& frame : frames) {
        auto line = BacktraceLocalThread::GetNativeFrameStr(frame);
        PrintStr(fd, line);
    }
    return true;
}

bool GetBacktrace(std::string& out)
{
    std::vector<NativeFrame> frames;
    if (!GetBacktraceFrames(frames)) {
        return false;
    }

    std::stringstream ss;
    for (auto const& frame : frames) {
        ss << BacktraceLocalThread::GetNativeFrameStr(frame);
    }
    out = ss.str();
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
