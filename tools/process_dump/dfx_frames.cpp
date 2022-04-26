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

#include "dfx_frames.h"

#include <cstdio>
#include <cstdlib>
#include <securec.h>

#include "dfx_elf.h"
#include "dfx_log.h"
#include "dfx_maps.h"

static const int FAULT_STACK_SHOW_FLOOR = 4;
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

void DfxFrames::SetFrameLr(uint64_t lr)
{
    lr_ = lr;
}

uint64_t DfxFrames::GetFrameLr() const
{
    return lr_;
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

void DfxFrames::SetFrameMapName(const std::string &mapName)
{
    frameMapName_ = mapName;
}

std::string DfxFrames::GetFrameMapName() const
{
    return frameMapName_;
}

void DfxFrames::SetFrameFaultStack(const std::string &faultStack)
{
    faultStack_ = faultStack;
}

std::string DfxFrames::GetFrameFaultStack() const
{
    return faultStack_;
}

void DfxFrames::DestroyFrames(const std::shared_ptr<DfxFrames> frameHead) {}

uint64_t DfxFrames::GetRelativePc(const std::shared_ptr<DfxElfMaps> head)
{
    DfxLogDebug("Enter %s.", __func__);
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
    DfxLogDebug("Exit %s.", __func__);
    return CalculateRelativePc(map);
}

uint64_t DfxFrames::CalculateRelativePc(std::shared_ptr<DfxElfMap> elfMap)
{
    DfxLogDebug("Enter %s.", __func__);
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
    DfxLogDebug("Exit %s.", __func__);
    return relativePc_;
}

std::string DfxFrames::PrintFrame() const
{
    DfxLogDebug("Enter %s.", __func__);

    char buf[LOG_BUF_LEN] = {0};

    std::string mapName = frameMapName_;
    if (mapName == "") {
        if (map_ == nullptr) {
            mapName = "Unknown";
        } else {
            mapName = map_->GetMapPath().c_str();
        }
    }

    if (funcName_ == "") {
        int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s\n", \
            index_, relativePc_, pc_, mapName.c_str());
        if (ret <= 0) {
            DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
        }
        DfxLogDebug("Exit %s.", __func__);
        return std::string(buf);
    }

    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, \
        "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s(%s+%" PRIu64 ")\n", index_, relativePc_, \
        pc_, mapName.c_str(), funcName_.c_str(), funcOffset_);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    DfxLogDebug("Exit %s.", __func__);
    return std::string(buf);
}

std::string DfxFrames::PrintFaultStack(int i) const
{
    DfxLogDebug("Enter %s.", __func__);

    if (faultStack_ == "") {
        return "";
    }

    char buf[LOG_BUF_LEN] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Sp%d:%s", i, faultStack_.c_str());
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }

    DfxLogDebug("Exit %s.", __func__);
    return std::string(buf);
}

std::string DfxFrames::ToString() const
{
    char buf[1024] = "\0"; // 1024 buffer length
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "#%02zu pc %016" PRIx64 " %s(%s+%" PRIu64 ")\n",
        index_,
        relativePc_,
        frameMapName_.c_str(),
        funcName_.c_str(),
        funcOffset_) <= 0) {
        return "Unknown";
    }
    return std::string(buf);
}

void PrintFrames(std::vector<std::shared_ptr<DfxFrames>> frames, int32_t fd)
{
    DfxLogDebug("Enter %s.", __func__);
    for (size_t i = 0; i < frames.size(); i++) {
        frames[i]->PrintFrame();
    }
    DfxLogDebug("Exit %s.", __func__);
}

std::string PrintFaultStacks(std::vector<std::shared_ptr<DfxFrames>> frames)
{
    DfxLogDebug("Enter %s.", __func__);

    std::string stackString = "";
    char buf[LOG_BUF_LEN] = {0};

    for (size_t i = 0; i < frames.size(); i++) {
        if (i == 0 && (frames[i]->GetFramePc() == 0)) {
            stackString = stackString + "Sp0: Unknow\n";
            continue;
        }
        if (i == FAULT_STACK_SHOW_FLOOR) {
            stackString = stackString + "    ...\n";
            break;
        }
        stackString = stackString + frames[i]->PrintFaultStack(i);
    }

    DfxLogDebug("Exit %s.", __func__);
    return stackString;
}
} // namespace HiviewDFX
} // namespace OHOS
