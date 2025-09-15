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
    std::string data;
    if (frame.isJsFrame) {
#if defined(ENABLE_MIXSTACK)
        data = StringPrintf("#%02zu at", frame.index);
        if (!frame.funcName.empty()) {
            data.append(" " + frame.funcName);
        }
        if (!frame.packageName.empty()) {
            data.append(" " + frame.packageName);
        }
        if (!frame.mapName.empty()) {
            data += StringPrintf(" (%s:%d:%d)", frame.mapName.c_str(), frame.line, frame.column);
        } else {
            std::string mapName = frame.map == nullptr ? "" : frame.map->name;
            data.append(" " + mapName);
        }
#endif
    } else {
        uint64_t pc = frame.relPc == 0 ? frame.pc : frame.relPc;
#ifdef __LP64__
        data = StringPrintf("#%02zu pc %016" PRIx64, frame.index, pc);
#else
        data = StringPrintf("#%02zu pc %08" PRIx64, frame.index, pc);
#endif
        if (!frame.mapName.empty()) {
            data += " " + DfxMap::UnFormatMapName(frame.mapName);
        } else {
            data += " [Unknown]";
        }
        if (!frame.funcName.empty() &&
            frame.funcName.length() <= MAX_FUNC_NAME_LEN) {
            data += "(" + frame.funcName;
            data += StringPrintf("+%" PRId64, frame.funcOffset);
            data += ")";
        }
        if (!frame.buildId.empty()) {
            data += "(" + frame.buildId + ")";
        }
    }
    data += "\n";
    return data;
}

std::string DfxFrameFormatter::GetFrameStr(const std::shared_ptr<DfxFrame>& frame)
{
    return frame ? GetFrameStr(*frame) : "";
}

std::string DfxFrameFormatter::GetFramesStr(const std::vector<DfxFrame>& frames)
{
    std::string ss;
    for (const auto& f : frames) {
        ss += GetFrameStr(f);
    }
    return ss;
}
} // namespace HiviewDFX
} // namespace OHOS
