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

#include "dfx_dump_catcher_frame.h"

#include <cstdio>
#include <cstdlib>
#include <securec.h>

#include "dfx_elf.h"
#include "dfx_log.h"
#include "dfx_maps.h"

#ifndef LOCAL_DUMPER_FRAME_DEBUG
#define LOCAL_DUMPER_FRAME_DEBUG
#endif
#undef LOCAL_DUMPER_FRAME_DEBUG

namespace OHOS {
namespace HiviewDFX {
DfxDumpCatcherFrame::DfxDumpCatcherFrame()
{
#ifdef LOCAL_DUMPER_FRAME_DEBUG
    DfxLogDebug("%{public}s :: construct.", __func__);
#endif
}

DfxDumpCatcherFrame::~DfxDumpCatcherFrame()
{
#ifdef LOCAL_DUMPER_FRAME_DEBUG
    DfxLogDebug("%{public}s :: destructor.", __func__);
#endif
}

void DfxDumpCatcherFrame::SetFrameIndex(size_t index)
{
    index_ = index;
}

size_t DfxDumpCatcherFrame::GetFrameIndex() const
{
    return index_;
}

void DfxDumpCatcherFrame::SetFrameFuncOffset(uint64_t funcOffset)
{
    funcOffset_ = funcOffset;
}

uint64_t DfxDumpCatcherFrame::GetFrameFuncOffset() const
{
    return funcOffset_;
}

void DfxDumpCatcherFrame::SetFramePc(uint64_t pc)
{
    pc_ = pc;
}

uint64_t DfxDumpCatcherFrame::GetFramePc() const
{
    return pc_;
}

void DfxDumpCatcherFrame::SetFrameLr(uint64_t lr)
{
    lr_ = lr;
}

uint64_t DfxDumpCatcherFrame::GetFrameLr() const
{
    return lr_;
}

void DfxDumpCatcherFrame::SetFrameSp(uint64_t sp)
{
    sp_ = sp;
}

uint64_t DfxDumpCatcherFrame::GetFrameSp() const
{
    return sp_;
}

void DfxDumpCatcherFrame::SetFrameRelativePc(uint64_t relativePc)
{
    relativePc_ = relativePc;
}

uint64_t DfxDumpCatcherFrame::GetFrameRelativePc() const
{
    return relativePc_;
}

void DfxDumpCatcherFrame::SetFrameMap(const std::shared_ptr<DfxElfMap> map)
{
    map_ = map;
}

std::shared_ptr<DfxElfMap> DfxDumpCatcherFrame::GetFrameMap() const
{
    return map_;
}

std::string DfxDumpCatcherFrame::ToString() const
{
    char buf[1024] = "\0"; // 1024 buffer length
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "#%02zu pc %016" PRIx64 " %s(%s+%" PRIu64 ")\n",
        index_,
        relativePc_,
        mapName_,
        funcName_.c_str(),
        funcOffset_) <= 0) {
        return "Unknown";
    }
    return std::string(buf);
}
} // namespace HiviewDFX
} // namespace OHOS
