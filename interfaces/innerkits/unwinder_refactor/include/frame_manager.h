/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FRAME_MANAGER_H
#define FRAME_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dfx_ark.h"
#include "dfx_frame.h"
#include "dfx_map.h"
#include "unwinder_shared_state.h"

namespace OHOS {
namespace HiviewDFX {

struct StepFrame {
    uintptr_t pc {0};
    uintptr_t sp {0};
    uintptr_t fp {0};
    bool isJsFrame {false};
    FrameType frameType {FrameType::NATIVE_FRAME};
};

class FrameManager {
public:
    explicit FrameManager(UnwinderSharedState& state) : state_(state)
    {}

    ~FrameManager() = default;

    void SetEnableFillFrames(bool enable)
    {
        enableFillFrames_ = enable;
    }

    void SetEnableParseNativeSymbol(bool enable)
    {
        enableParseNativeSymbol_ = enable;
    }

    bool GetEnableParseNativeSymbol() const
    {
        return enableParseNativeSymbol_;
    }

    void Clear();

    void AddFrame(const StepFrame& frame, std::shared_ptr<DfxMap> map);

    void AddFrame(DfxFrame& frame)
    {
        frames_.emplace_back(frame);
    }

    void SetFrames(const std::vector<DfxFrame>& frames)
    {
        frames_ = frames;
    }

    const std::vector<DfxFrame>& GetFrames();

    const std::vector<uintptr_t>& GetPcs() const
    {
        return pcs_;
    }

    size_t GetFrameCount() const
    {
        return frames_.size();
    }

    void FillFrames(std::vector<DfxFrame>& frames);
    void FillFrame(DfxFrame& frame, bool needSymParse = true);
    void ParseFrameSymbol(DfxFrame& frame);

    bool GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame);
    void GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs);

    bool GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps,
        std::string& funcName, uint64_t& funcOffset);

    void FillJsFrame(DfxFrame& frame);
    void FillArkwebJsFrame(DfxFrame& frame);

private:
    void FillSingleFrame(DfxFrame& frame);
    void AppendAdltOriginName(DfxFrame& frame, std::string& mapName,
        const std::shared_ptr<DfxElf>& elf);
    bool EnsureArkwebExtractor(uintptr_t& extractorPtr, int32_t pid);
    void FillArkwebJsFrameInfo(DfxFrame& frame, uintptr_t extractorPtr);
    bool FillHapJsFrame(DfxFrame& frame, JsFunction& jsFunction);
    bool CollectJsFunction(DfxFrame& frame, JsFunction& jsFunction);
    void FillJsFrameFields(DfxFrame& frame, const JsFunction& jsFunction);
    bool FindMapForFrame(std::shared_ptr<DfxMaps> maps, uintptr_t pc,
        std::shared_ptr<DfxMap>& map);
    void FillFrameByMap(DfxFrame& frame);
    void BuildFrameForPc(std::vector<DfxFrame>& frames, std::vector<uintptr_t>& pcs,
        size_t i, std::shared_ptr<DfxMap>& map);
    bool IsMapMatched(uintptr_t pc, const std::shared_ptr<DfxMap>& map);
    bool IsArkWebJsMap(const std::shared_ptr<DfxMap>& map);
    bool ParseSymbolByPc(uintptr_t pc, const std::shared_ptr<DfxMap>& map,
        std::string& funcName, uint64_t& funcOffset);

public:
    UnwinderSharedState& state_;
    bool enableFillFrames_ {true};
    bool enableParseNativeSymbol_ {true};
    std::vector<uintptr_t> pcs_ {};
    std::vector<DfxFrame> frames_ {};
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // FRAME_MANAGER_H
