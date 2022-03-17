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
#ifndef DFX_DUMP_CATCHER_FRAME_H
#define DFX_DUMP_CATCHER_FRAME_H

#include <memory>
#include <string>

#include "dfx_elf.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
namespace OHOS {
namespace HiviewDFX {
class DfxDumpCatcherFrame {
public:
    DfxDumpCatcherFrame();
    ~DfxDumpCatcherFrame();

    uint64_t GetRelativePc(const std::shared_ptr<DfxElfMaps> head);
    uint64_t CalculateRelativePc(std::shared_ptr<DfxElfMap> elfMap);
    void SetFrameIndex(size_t index);
    size_t GetFrameIndex() const;
    void SetFrameFuncOffset(uint64_t funcOffset);
    uint64_t GetFrameFuncOffset() const;
    void SetFramePc(uint64_t pc);
    uint64_t GetFramePc() const;
    void SetFrameLr(uint64_t lr);
    uint64_t GetFrameLr() const;
    void SetFrameSp(uint64_t sp);
    uint64_t GetFrameSp() const;
    void SetFrameRelativePc(uint64_t relativePc);
    uint64_t GetFrameRelativePc() const;
    void SetFrameMap(const std::shared_ptr<DfxElfMap> map);
    std::shared_ptr<DfxElfMap> GetFrameMap() const;
    std::string ToString() const;
    size_t index_ = 0;
    uint64_t funcOffset_ = 0;
    uint64_t pc_ = 0;
    uint64_t lr_ = 0;
    uint64_t sp_ = 0;
    uint64_t relativePc_ = 0;
    char funcName_[1024] = { 0 }; // 1024 : max func name
    std::shared_ptr<DfxElfMap> map_ = nullptr; // managed in DfxProcess class
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
