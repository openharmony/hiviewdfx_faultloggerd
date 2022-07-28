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
#ifndef DFX_FRAME_H
#define DFX_FRAME_H

#include <stddef.h>    // for size_t
#include <stdint.h>    // for uint64_t
#include <memory>      // for shared_ptr
#include <string>      // for basic_string
#include "dfx_maps.h"  // for DfxElfMap, DfxElfMaps
#include "iosfwd"      // for string
#include "vector"      // for vector

namespace OHOS {
namespace HiviewDFX {
class DfxFrame {
public:
    DfxFrame() = default;
    ~DfxFrame() = default;

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
    void SetFrameFuncName(const std::string &funcName);
    std::string GetFrameFuncName() const;
    void SetFrameMap(const std::shared_ptr<DfxElfMap> map);
    std::shared_ptr<DfxElfMap> GetFrameMap() const;
    void SetFrameFaultStack(const std::string &faultStack);
    std::string GetFrameFaultStack() const;
    void SetFrameMapName(const std::string &mapName);
    std::string GetFrameMapName() const;

    std::string ToString() const;
    uint64_t GetRelativePc(const std::shared_ptr<DfxElfMaps> head);
    uint64_t CalculateRelativePc(std::shared_ptr<DfxElfMap> elfMap);
    std::string PrintFrame() const;
    std::string PrintFaultStack(int i) const;

private:
    size_t index_ = 0;
    uint64_t funcOffset_ = 0;
    uint64_t pc_ = 0;
    uint64_t lr_ = 0;
    uint64_t sp_ = 0;
    uint64_t relativePc_ = 0;
    std::string funcName_;
    std::string frameMapName_ = "";
    std::shared_ptr<DfxElfMap> map_ = nullptr; // managed in DfxProcess class
    std::string faultStack_;
};

void PrintFrames(std::vector<std::shared_ptr<DfxFrame>> frames);
std::string PrintFaultStacks(std::vector<std::shared_ptr<DfxFrame>> frames);
} // namespace HiviewDFX
} // namespace OHOS

#endif
