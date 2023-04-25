/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "dfx_frame.h"
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <ostream>
#include <securec.h>
#include <sstream>
#include <string>
#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_log.h"
#include "dfx_maps.h"

namespace OHOS {
namespace HiviewDFX {
void DfxFrame::SetFrameIndex(size_t index)
{
    index_ = index;
}

size_t DfxFrame::GetFrameIndex() const
{
    return index_;
}

void DfxFrame::SetFrameFuncOffset(uint64_t funcOffset)
{
    funcOffset_ = funcOffset;
}

uint64_t DfxFrame::GetFrameFuncOffset() const
{
    return funcOffset_;
}

void DfxFrame::SetFramePc(uint64_t pc)
{
    pc_ = pc;
}

uint64_t DfxFrame::GetFramePc() const
{
    return pc_;
}

void DfxFrame::SetFrameLr(uint64_t lr)
{
    lr_ = lr;
}

uint64_t DfxFrame::GetFrameLr() const
{
    return lr_;
}

void DfxFrame::SetFrameSp(uint64_t sp)
{
    sp_ = sp;
}

uint64_t DfxFrame::GetFrameSp() const
{
    return sp_;
}

void DfxFrame::SetFrameRelativePc(uint64_t relativePc)
{
    relativePc_ = relativePc;
}

uint64_t DfxFrame::GetFrameRelativePc() const
{
    return relativePc_;
}

void DfxFrame::SetFrameFuncName(const std::string &funcName)
{
    funcName_ = funcName;
}

std::string DfxFrame::GetFrameFuncName() const
{
    return funcName_;
}

void DfxFrame::SetFrameMap(const std::shared_ptr<DfxElfMap> map)
{
    map_ = map;
}

std::shared_ptr<DfxElfMap> DfxFrame::GetFrameMap() const
{
    return map_;
}

void DfxFrame::SetFrameMapName(const std::string &mapName)
{
    frameMapName_ = mapName;
}

std::string DfxFrame::GetFrameMapName() const
{
    return frameMapName_;
}

void DfxFrame::SetBuildId(const std::string &buildId)
{
    buildId_ = buildId;
}

std::string DfxFrame::GetBuildId() const
{
    return buildId_;
}

uint64_t DfxFrame::GetRelativePc(const std::shared_ptr<DfxElfMaps> head)
{
    if (head == nullptr) {
        return 0;
    }

    if (map_ == nullptr) {
        if (!head->FindMapByAddr(pc_, map_)) {
            return 0;
        }
    }

    if (!map_->IsValid()) {
        DFXLOG_WARN("No elf map:%s.", map_->GetMapPath().c_str());
        return 0;
    }

    return CalcRelativePc(map_);
}

uint64_t DfxFrame::CalcRelativePc(std::shared_ptr<DfxElfMap> elfMap)
{
    if (elfMap == nullptr) {
        return 0;
    }
    relativePc_ = elfMap->GetRelPc(pc_);
    return relativePc_;
}

std::string DfxFrame::ToString() const
{
    char buf[LOG_BUF_LEN] = {0}; // 1024 buffer length
#ifdef __LP64__
    char format[] = "#%02zu pc %016" PRIx64 " %s";
#else
    char format[] = "#%02zu pc %08" PRIx64 " %s";
#endif
    std::string mapName = frameMapName_;
    if (mapName.empty()) {
        mapName = "Unknown";
    }

    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format,
        index_,
        relativePc_,
        mapName.c_str()) <= 0) {
        DFXLOG_ERROR("%s :: snprintf_s failed, mapName: %s", __func__, mapName.c_str());
    }

    std::ostringstream ss;
    ss << std::string(buf, strlen(buf));
    if (!funcName_.empty()) {
        ss << "(";
        ss << funcName_;
        ss << "+" << funcOffset_ << ")";
    }
    if (!buildId_.empty()) {
        ss << "(" << buildId_ << ")";
    }
    ss << std::endl;
    return ss.str();
}

void PrintFrames(std::vector<std::shared_ptr<DfxFrame>> frames)
{
    for (size_t i = 0; i < frames.size(); i++) {
        frames[i]->ToString();
    }
}
} // namespace HiviewDFX
} // namespace OHOS
