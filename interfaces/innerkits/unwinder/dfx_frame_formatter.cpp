/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "dfx_frame_formatter.h"
#include <sstream>
#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxFrameFormatter"
}

std::string DfxFrameFormatter::GetFrameStr(const DfxFrame& frame)
{
    return GetFrameStr(std::make_shared<DfxFrame>(frame));
}

std::string DfxFrameFormatter::GetFrameStr(const std::shared_ptr<DfxFrame>& frame)
{
    std::string data;
    if (frame->isJsFrame) {
#if defined(ENABLE_MIXSTACK)
        data = StringPrintf("#%02zu at %s (%s:%d:%d)", frame->index, frame->funcName.c_str(), frame->mapName.c_str(),
            frame->line, frame->column);
#endif
    } else {
#ifdef __LP64__
        data = StringPrintf("#%02zu pc %016" PRIx64, frame->index, frame->relPc);
#else
        data = StringPrintf("#%02zu pc %08" PRIx64, frame->index, frame->relPc);
#endif
        if (!frame->mapName.empty()) {
            DfxMaps::UnFormatMapName(frame->mapName);
            data += " " + frame->mapName;
        } else {
            data += " [Unknown]";
        }

        if (!frame->funcName.empty()) {
            data += "(" + frame->funcName;
            data += StringPrintf("+%" PRId64, frame->funcOffset);
            data += ")";
        }
        if (!frame->buildId.empty()) {
            data += "(" + frame->buildId + ")";
        }
    }
    data += "\n";
    return data;
}

std::string DfxFrameFormatter::GetFramesStr(const std::vector<DfxFrame>& frames)
{
    if (frames.size() == 0) {
        return "";
    }
    std::ostringstream ss;
    for (const auto& f : frames) {
        ss << GetFrameStr(f);
    }
    return ss.str();
}

std::string DfxFrameFormatter::GetFramesStr(const std::vector<std::shared_ptr<DfxFrame>>& frames)
{
    if (frames.size() == 0) {
        return "";
    }
    std::ostringstream ss;
    for (const auto& pf : frames) {
        ss << GetFrameStr(pf);
    }
    return ss.str();
}

std::vector<std::shared_ptr<DfxFrame>> DfxFrameFormatter::ConvertFrames(const std::vector<DfxFrame>& frames)
{
    std::vector<std::shared_ptr<DfxFrame>> pFrames;
    for (const auto& frame : frames) {
        pFrames.emplace_back(std::make_shared<DfxFrame>(frame));
    }
    return pFrames;
}
} // namespace HiviewDFX
} // namespace OHOS
