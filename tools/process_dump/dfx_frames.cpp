/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "dfx_frames.h"

#include <cstdio>
#include <cstdlib>

#include "dfx_elf.h"
#include "dfx_log.h"
#include "dfx_maps.h"

namespace OHOS {
namespace HiviewDFX {
void DfxFrames::SetFrameIndex(size_t index)
{
    index_ = index;
}

size_t DfxFrames::GetFrameIndex() const
{
    return index_;
}

void DfxFrames::SetFrameFuncOffset(uint64_t funcOffset)
{
    funcOffset_ = funcOffset;
}

uint64_t DfxFrames::GetFrameFuncOffset() const
{
    return funcOffset_;
}

void DfxFrames::SetFramePc(uint64_t pc)
{
    pc_ = pc;
}

uint64_t DfxFrames::GetFramePc() const
{
    return pc_;
}

void DfxFrames::SetFrameSp(uint64_t sp)
{
    sp_ = sp;
}

uint64_t DfxFrames::GetFrameSp() const
{
    return sp_;
}

void DfxFrames::SetFrameRelativePc(uint64_t relativePc)
{
    relativePc_ = relativePc;
}

uint64_t DfxFrames::GetFrameRelativePc() const
{
    return relativePc_;
}

void DfxFrames::SetFrameFuncName(const std::string &funcName)
{
    funcName_ = funcName;
}

std::string DfxFrames::GetFrameFuncName() const
{
    return funcName_;
}

void DfxFrames::SetFrameMap(const std::shared_ptr<DfxElfMap> map)
{
    map_ = map;
}

std::shared_ptr<DfxElfMap> DfxFrames::GetFrameMap() const
{
    return map_;
}

void DfxFrames::DestroyFrames(const std::shared_ptr<DfxFrames> frameHead) {}

uint64_t DfxFrames::GetRelativePc(const std::shared_ptr<DfxElfMaps> head)
{
    DfxLogInfo("Enter %s.", __func__);
    if (head == nullptr) {
        return 0;
    }

    if (map_ == nullptr) {
        if (!head->FindMapByAddr(pc_, map_)) {
            return 0;
        }
    }

    if (!map_->IsVaild()) {
        DfxLogWarn("No elf map:%s.", map_->GetMapPath().c_str());
        return 0;
    }

    std::shared_ptr<DfxElfMap> map = nullptr;
    if (!head->FindMapByPath(map_->GetMapPath(), map)) {
        DfxLogWarn("Fail to find Map:%s.", map_->GetMapPath().c_str());
        return 0;
    }
    DfxLogInfo("Exit %s.", __func__);
    return CalculateRelativePc(map);
}

uint64_t DfxFrames::CalculateRelativePc(std::shared_ptr<DfxElfMap> elfMap)
{
    DfxLogInfo("Enter %s.", __func__);
    if (elfMap == nullptr || map_ == nullptr) {
        return 0;
    }

    if (elfMap->GetMapImage() == nullptr) {
        elfMap->SetMapImage(DfxElf::Create(elfMap->GetMapPath().c_str()));
    }

    if (elfMap->GetMapImage() == nullptr) {
        relativePc_ = pc_ - (map_->GetMapBegin() - map_->GetMapOffset());
    } else {
        relativePc_ = (pc_ - map_->GetMapBegin()) + elfMap->GetMapImage()->FindRealLoadOffset(map_->GetMapOffset());
    }

#ifdef __aarch64__
    relativePc_ = relativePc_ - 4; // 4 : instr offset
#elif defined(__x86_64__)
    relativePc_ = relativePc_ - 1; // 1 : instr offset
#endif
    DfxLogInfo("Exit %s.", __func__);
    return relativePc_;
}

void DfxFrames::PrintFrame(const int32_t fd) const
{
    DfxLogInfo("Enter %s.", __func__);
    if (pc_ == 0) {
        return;
    }

    if (funcName_ == "") {
        WriteLog(fd, "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s\n", index_, relativePc_,
            pc_, (map_ == nullptr) ? "Unknown" : map_->GetMapPath().c_str());
        return;
    }

    WriteLog(fd, "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s(%s+%" PRIu64 ")\n", index_, relativePc_,
        pc_, (map_ == nullptr) ? "Unknown" : map_->GetMapPath().c_str(), funcName_.c_str(), funcOffset_);
    DfxLogInfo("Exit %s.", __func__);
}

void PrintFrames(std::vector<std::shared_ptr<DfxFrames>> frames, int32_t fd)
{
    DfxLogInfo("Enter %s.", __func__);
    for (size_t i = 0; i < frames.size(); i++) {
        frames[i]->PrintFrame(fd);
    }
    DfxLogInfo("Exit %s.", __func__);
}
} // namespace HiviewDFX
} // namespace OHOS
