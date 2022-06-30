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

#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <securec.h>

#include "dfx_elf.h"
#include "dfx_logger.h"
#include "dfx_maps.h"

static const int FAULT_STACK_SHOW_FLOOR = 4;
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

void DfxFrame::SetFrameFaultStack(const std::string &faultStack)
{
    faultStack_ = faultStack;
}

std::string DfxFrame::GetFrameFaultStack() const
{
    return faultStack_;
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
        DfxLogWarn("No elf map:%s.", map_->GetMapPath().c_str());
        return 0;
    }

    std::shared_ptr<DfxElfMap> map = nullptr;
    if (!head->FindMapByPath(map_->GetMapPath(), map)) {
        DfxLogWarn("Fail to find Map:%s.", map_->GetMapPath().c_str());
        return 0;
    }
    return CalculateRelativePc(map);
}

uint64_t DfxFrame::CalculateRelativePc(std::shared_ptr<DfxElfMap> elfMap)
{
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
    return relativePc_;
}

std::string DfxFrame::PrintFrame() const
{
    char buf[LOG_BUF_LEN] = {0};

    std::string mapName = frameMapName_;
    if (mapName.empty()) {
        mapName = "Unknown";
    }

#ifdef __LP64__
    char frameFormatWithMapName[] = "#%02zu pc %016" PRIx64 " %s\n";
    char frameFormatWithFuncName[] = "#%02zu pc %016" PRIx64 " %s(%s+%" PRIu64 ")\n";
#else
    char frameFormatWithMapName[] = "#%02zu pc %08" PRIx64 " %s\n";
    char frameFormatWithFuncName[] = "#%02zu pc %08" PRIx64 " %s(%s+%" PRIu64 ")\n";
#endif

    if (funcName_.empty()) {
        int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, frameFormatWithMapName, \
            index_, relativePc_, mapName.c_str());
        if (ret <= 0) {
            DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
        }
        return std::string(buf);
    }

    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, \
        frameFormatWithFuncName, index_, relativePc_, \
        mapName.c_str(), funcName_.c_str(), funcOffset_);
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    return std::string(buf);
}

std::string DfxFrame::PrintFaultStack(int i) const
{
    if (faultStack_ == "") {
        return "";
    }

    std::stringstream ss;
    ss << "Sp" << i;
    ss << ":" << faultStack_;
    return ss.str();
}

std::string DfxFrame::ToString() const
{
    char buf[1024] = "\0"; // 1024 buffer length
#ifdef __LP64__
    char format[] = "#%02zu pc %016" PRIx64 " %s";
#else
    char format[] = "#%02zu pc %08" PRIx64 " %s";
#endif
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format,
        index_,
        relativePc_,
        frameMapName_.c_str()) <= 0) {
        return "Unknown";
    }

    std::ostringstream ss;
    ss << std::string(buf, strlen(buf));
    if (funcName_.empty()) {
        ss << std::endl;
    } else {
        ss << "(";
        ss << funcName_;
        ss << "+" << funcOffset_ << ")" << std::endl;
    }
    return ss.str();
}

void PrintFrames(std::vector<std::shared_ptr<DfxFrame>> frames)
{
    for (size_t i = 0; i < frames.size(); i++) {
        frames[i]->PrintFrame();
    }
}

std::string PrintFaultStacks(std::vector<std::shared_ptr<DfxFrame>> frames)
{
    std::string stackString = "";

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
    
    return stackString;
}
} // namespace HiviewDFX
} // namespace OHOS
