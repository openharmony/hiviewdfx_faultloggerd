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
#include "frame_manager.h"

#include <cstring>

#include "dfx_ark.h"
#include "dfx_define.h"
#include "dfx_frame_formatter.h"
#include "dfx_hap.h"
#include "dfx_instructions.h"
#include "dfx_jsvm.h"
#include "dfx_log.h"
#include "dfx_symbols.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#include "local_unwinder.h"
#include "string_util.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxFrameManager"
constexpr const char* NOT_MAPPED = "Not mapped";
constexpr const char* ARKWEB_NATIVE_MAP = "libarkwebcore.so";
}

void FrameManager::Clear()
{
    pcs_.clear();
    frames_.clear();
    state_.lastErrorData = {};
}

void FrameManager::AddFrame(const StepFrame& frame, std::shared_ptr<DfxMap> map)
{
#if defined(ENABLE_MIXSTACK)
    if (state_.ignoreMixstack && frame.isJsFrame) {
        return;
    }
#endif
    pcs_.emplace_back(frame.pc);
    DfxFrame dfxFrame;
    if (IsArkWebJsMap(map)) {
        dfxFrame.isJsFrame = true;
        dfxFrame.frameType = FrameType::ARKWEB_JS_FRAME;
    } else {
        dfxFrame.isJsFrame = frame.isJsFrame;
        dfxFrame.frameType = frame.frameType;
    }
    dfxFrame.index = frames_.size();
    dfxFrame.fp = static_cast<uint64_t>(frame.fp);
    dfxFrame.pc = static_cast<uint64_t>(frame.pc);
    dfxFrame.sp = static_cast<uint64_t>(frame.sp);
    dfxFrame.map = map;
    frames_.emplace_back(dfxFrame);
}

bool FrameManager::IsArkWebJsMap(const std::shared_ptr<DfxMap>& map)
{
    return (map != nullptr) && map->IsArkWebJsExecutable();
}

const std::vector<DfxFrame>& FrameManager::GetFrames()
{
    if (enableFillFrames_) {
        FillFrames(frames_);
    }
    return frames_;
}

void FrameManager::FillFrames(std::vector<DfxFrame>& frames)
{
    for (size_t i = 0; i < frames.size(); ++i) {
        FillSingleFrame(frames[i]);
    }
}

void FrameManager::FillSingleFrame(DfxFrame& frame)
{
    if (frame.frameType == FrameType::JS_FRAME ||
        frame.frameType == FrameType::STATIC_JS_FRAME) {
#if defined(ENABLE_MIXSTACK)
        FillJsFrame(frame);
#endif
        return;
    }
    if (frame.frameType == FrameType::ARKWEB_JS_FRAME) {
        FillArkwebJsFrame(frame);
        return;
    }
    FillFrame(frame, enableParseNativeSymbol_);
}

void FrameManager::ParseFrameSymbol(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        return;
    }
    pid_t pid = state_.GetRealPid();
    auto elf = frame.map->GetElf(pid);
    if (elf == nullptr) {
        return;
    }
    if (!DfxSymbols::GetFuncNameAndOffsetByPc(frame.relPc, elf, frame.funcName, frame.funcOffset)) {
        DFXLOGU("Failed to get symbol, relPc: 0x%{public}" PRIx64 ", pc: 0x%{public}" PRIx64 "",
            frame.relPc, frame.pc);
    }
    frame.parseSymbolState.SetParseSymbolState(true);
}

void FrameManager::FillFrame(DfxFrame& frame, bool needSymParse)
{
    if (frame.map == nullptr) {
        frame.relPc = frame.pc;
        frame.mapName = NOT_MAPPED;
        DFXLOGU("Current frame is not mapped.");
        return;
    }
    std::string mapName = frame.map->GetElfName();
    frame.mapName = mapName;
    DFX_TRACE_SCOPED_DLSYM("FillFrame:%s", frame.mapName.c_str());
    pid_t pid = state_.GetRealPid();
    auto elf = frame.map->GetElf(pid);
    frame.relPc = frame.map->GetRelPc(frame.pc);
    frame.mapOffset = frame.map->offset;
    if (elf == nullptr) {
        return;
    }
    frame.buildId = elf->GetBuildId();
    if (needSymParse) {
        ParseFrameSymbol(frame);
    }
    AppendAdltOriginName(frame, mapName, elf);
    frame.mapName = mapName;
}

void FrameManager::AppendAdltOriginName(DfxFrame& frame, std::string& mapName,
    const std::shared_ptr<DfxElf>& elf)
{
    if (!elf->IsAdlt()) {
        return;
    }
    std::string originSoName = elf->GetAdltOriginSoNameByRelPc(frame.relPc);
    if (!originSoName.empty()) {
        mapName.append(":" + originSoName);
    }
}

void FrameManager::FillArkwebJsFrame(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        DFXLOGU("Current ArkwebJs frame is not map.");
        return;
    }
    DFX_TRACE_SCOPED_DLSYM("FillArkwebJsFrame:%s", frame.map->name.c_str());
    int32_t pid = state_.GetRealPid();
    if (pid <= 0) {
        DFXLOGE("pid can not less than 0, return directly!");
        return;
    }
    uintptr_t extractorPtr = state_.arkwebJsExtractorptr;
    if (!EnsureArkwebExtractor(extractorPtr, pid)) {
        DFXLOGE("create arkweb js extractor failed");
        return;
    }
    state_.arkwebJsExtractorptr = extractorPtr;
    FillArkwebJsFrameInfo(frame, extractorPtr);
}

bool FrameManager::EnsureArkwebExtractor(uintptr_t& extractorPtr, int32_t pid)
{
    if (extractorPtr != 0) {
        return true;
    }
    return DfxJsvm::Instance().JsvmCreateJsSymbolExtractor(
        &extractorPtr, static_cast<uint32_t>(pid)) != -1;
}

void FrameManager::FillArkwebJsFrameInfo(DfxFrame& frame, uintptr_t extractorPtr)
{
    JsvmFunction jsFunction;
    DfxJsvm::Instance().ParseJsvmFrameInfo(frame.pc, extractorPtr, &jsFunction);
    frame.funcName = std::string(jsFunction.functionName);
    if (StartsWith(frame.map->name, ARKWEB_NATIVE_MAP)) {
        frame.mapName = frame.map->GetElfName();
        frame.isJsFrame = false;
        frame.relPc = frame.pc - frame.map->begin;
        frame.funcOffset = jsFunction.offsetInFunction;
    } else {
        frame.isJsFrame = true;
        frame.mapName = std::string(jsFunction.url);
        frame.line = jsFunction.line;
        frame.column = jsFunction.column;
    }
}

void FrameManager::FillJsFrame(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        DFXLOGU("Current js frame is not map.");
        return;
    }
    DFX_TRACE_SCOPED_DLSYM("FillJsFrame:%s", frame.map->name.c_str());
    JsFunction jsFunction {};
    if (!CollectJsFunction(frame, jsFunction)) {
        return;
    }
    FillJsFrameFields(frame, jsFunction);
}

bool FrameManager::CollectJsFunction(DfxFrame& frame, JsFunction& jsFunction)
{
    if (state_.IsLocal()) {
        return LocalUnwinder::FillJsFrame(frame, &jsFunction);
    }
    return FillHapJsFrame(frame, jsFunction);
}

bool FrameManager::FillHapJsFrame(DfxFrame& frame, JsFunction& jsFunction)
{
    auto hap = frame.map->GetHap();
    if (hap == nullptr) {
        DFXLOGW("Get hap error, name: %{public}s", frame.map->name.c_str());
        return false;
    }
    if (!hap->ParseHapInfo(state_.pid, frame.pc, frame.map, &jsFunction)) {
        DFXLOGW("Failed to parse hap info, pid: %{public}d", state_.pid);
        return false;
    }
    return true;
}

void FrameManager::FillJsFrameFields(DfxFrame& frame, const JsFunction& jsFunction)
{
    frame.isJsFrame = true;
    frame.mapName = std::string(jsFunction.url, strnlen(jsFunction.url, sizeof(jsFunction.url)));
    frame.funcName = std::string(jsFunction.functionName,
        strnlen(jsFunction.functionName, sizeof(jsFunction.functionName)));
    frame.packageName = std::string(jsFunction.packageName,
        strnlen(jsFunction.packageName, sizeof(jsFunction.packageName)));
    frame.line = static_cast<int32_t>(jsFunction.line);
    frame.column = jsFunction.column;
}

bool FrameManager::GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame)
{
    frame.pc = static_cast<uint64_t>(StripPac(pc, 0));
    std::shared_ptr<DfxMap> map = nullptr;
    if (!FindMapForFrame(maps, frame.pc, map)) {
        DFXLOGE("Find map error");
        return false;
    }
    frame.map = map;
    FillFrameByMap(frame);
    return true;
}

bool FrameManager::FindMapForFrame(std::shared_ptr<DfxMaps> maps, uintptr_t pc,
    std::shared_ptr<DfxMap>& map)
{
    return (maps != nullptr) && maps->FindMapByAddr(pc, map) && (map != nullptr);
}

void FrameManager::FillFrameByMap(DfxFrame& frame)
{
    if (DfxMaps::IsArkHapMapItem(frame.map->name) ||
        DfxMaps::IsArkCodeMapItem(frame.map->name)) {
        FillJsFrame(frame);
        return;
    }
    if (frame.map->IsArkWebJsExecutable()) {
        FillArkwebJsFrame(frame);
        return;
    }
    FillFrame(frame, enableParseNativeSymbol_);
}

void FrameManager::GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
{
    if (state_.maps == nullptr) {
        DFXLOGE("maps_ is null, return directly!");
        return;
    }
    frames.clear();
    std::shared_ptr<DfxMap> map = nullptr;
    for (size_t i = 0; i < pcs.size(); ++i) {
        BuildFrameForPc(frames, pcs, i, map);
    }
}

void FrameManager::BuildFrameForPc(std::vector<DfxFrame>& frames, std::vector<uintptr_t>& pcs,
    size_t i, std::shared_ptr<DfxMap>& map)
{
    DfxFrame frame;
    frame.index = i;
    pcs[i] = StripPac(pcs[i], 0);
    frame.pc = static_cast<uint64_t>(pcs[i]);
    if (IsMapMatched(frame.pc, map)) {
        DFXLOGU("map had matched");
    } else if (!state_.maps->FindMapByAddr(pcs[i], map)) {
        map = nullptr;
        frame.relPc = frame.pc;
        frame.mapName = NOT_MAPPED;
        DFXLOGE("Find map error");
        return;
    }
    frame.map = map;
    FillFrameByMap(frame);
    frames.emplace_back(frame);
}

bool FrameManager::IsMapMatched(uintptr_t pc, const std::shared_ptr<DfxMap>& map)
{
    return (map != nullptr) && map->Contain(pc);
}

bool FrameManager::GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps,
    std::string& funcName, uint64_t& funcOffset)
{
    if (maps == nullptr) {
        return false;
    }
    std::shared_ptr<DfxMap> map = nullptr;
    if (!FindMapForFrame(maps, pc, map)) {
        DFXLOGE("Find map is null");
        return false;
    }
    return ParseSymbolByPc(pc, map, funcName, funcOffset);
}

bool FrameManager::ParseSymbolByPc(uintptr_t pc, const std::shared_ptr<DfxMap>& map,
    std::string& funcName, uint64_t& funcOffset)
{
    uint64_t relPc = map->GetRelPc(static_cast<uint64_t>(pc));
    auto elf = map->GetElf();
    if (elf == nullptr) {
        DFXLOGE("Get elf is null");
        return false;
    }
    return DfxSymbols::GetFuncNameAndOffsetByPc(relPc, elf, funcName, funcOffset);
}

} // namespace HiviewDFX
} // namespace OHOS
