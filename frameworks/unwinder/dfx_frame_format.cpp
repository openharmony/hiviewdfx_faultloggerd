/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

/* This files contains processdump frame module. */

#include "dfx_frame_format.h"
#include <ostream>
#include <securec.h>
#include <sstream>
#include "dfx_log.h"
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
const int FRAME_BUF_LEN = 1024;

std::string DfxFrameFormat::GetFrameStr(const DfxFrame& frame)
{
    return GetFrameStr(std::make_shared<DfxFrame>(frame));
}

std::string DfxFrameFormat::GetFrameStr(const std::shared_ptr<DfxFrame>& frame)
{
    char buf[FRAME_BUF_LEN] = {0};
#ifdef __LP64__
    char format[] = "#%02zu pc %016" PRIx64 " %s";
#else
    char format[] = "#%02zu pc %08" PRIx64 " %s";
#endif

    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format,
        frame->index,
        frame->relPc,
        frame->mapName.empty() ? "Unknown" : frame->mapName.c_str()) <= 0) {
        DFXLOG_ERROR("%s :: snprintf_s failed, mapName: %s", __func__, frame->mapName.c_str());
        return "[Unknown]";
    }

    std::ostringstream ss;
    ss << std::string(buf, strlen(buf));
    if (!frame->funcName.empty()) {
        ss << "(";
        ss << frame->funcName.c_str();
        ss << "+" << frame->funcOffset << ")";
    }
    if (!frame->buildId.empty()) {
        ss << "(" << frame->buildId << ")";
    }
    ss << std::endl;
    return ss.str();
}

std::string DfxFrameFormat::GetFramesStr(const std::vector<DfxFrame>& frames)
{
    std::ostringstream ss;
    for (const auto& frame : frames) {
        ss << GetFrameStr(frame);
    }
    return ss.str();
}

std::string DfxFrameFormat::GetFramesStr(const std::vector<std::shared_ptr<DfxFrame>>& frames)
{
    std::ostringstream ss;
    for (const auto& frame : frames) {
        ss << GetFrameStr(frame);
    }
    return ss.str();
}

std::vector<std::shared_ptr<DfxFrame>> DfxFrameFormat::ConvertFrames(const std::vector<DfxFrame>& frames)
{
    std::vector<std::shared_ptr<DfxFrame>> ptrFrames;
    for (const auto& frame : frames) {
        ptrFrames.push_back(std::make_shared<DfxFrame>(frame));
    }
    return ptrFrames;
}
} // namespace HiviewDFX
} // namespace OHOS
