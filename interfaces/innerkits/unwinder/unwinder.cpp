/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "unwinder.h"

#include <dlfcn.h>
#include <link.h>

#include "dfx_ark.h"
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_extractor_utils.h"
#include "dfx_frame_formatter.h"
#include "dfx_instructions.h"
#include "dfx_log.h"
#include "dfx_regs_get.h"
#include "dfx_symbols.h"
#include "stack_util.h"
#include "string_printf.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxUnwinder"
}

void Unwinder::Init()
{
    Destroy();
    memory_ = std::make_shared<DfxMemory>(acc_);
#if defined(__arm__)
    armExidx_ = std::make_shared<ArmExidx>(memory_);
#endif
    dwarfSection_ = std::make_shared<DwarfSection>(memory_);

    if (pid_ == UNWIND_TYPE_LOCAL) {
        maps_ = DfxMaps::Create(getpid());
    } else {
        if (pid_ > 0) {
            maps_ = DfxMaps::Create(pid_);
        }
    }
}

void Unwinder::Clear()
{
    isFpStep_ = false;
    pcs_.clear();
    frames_.clear();
    if (memset_s(&lastErrorData_, sizeof(UnwindErrorData), 0, sizeof(UnwindErrorData)) != 0) {
        LOGE("Failed to memset lastErrorData");
    }
}

void Unwinder::Destroy()
{
    Clear();
    rsCache_.clear();
}

bool Unwinder::CheckAndReset(void* ctx)
{
    if ((ctx == nullptr) || (memory_ == nullptr)) {
        return false;
    }
    memory_->SetCtx(ctx);
    SetLocalStackCheck(ctx, false);
    return true;
}

bool Unwinder::GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    if (getpid() == gettid()) {
        if (maps_ == nullptr || !maps_->GetStackRange(stackBottom, stackTop)) {
            return false;
        }
    } else {
        if (GetSelfStackRange(stackBottom, stackTop) != 0) {
            return false;
        }
    }
    return true;
}

bool Unwinder::UnwindLocalWithContext(const ucontext_t& context, size_t maxFrameNum, size_t skipFrameNum)
{
    regs_ = DfxRegs::CreateFromUcontext(context);
    return UnwindLocal(true, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindLocal(bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    if ((maps_ == nullptr) || !GetStackRange(stackBottom, stackTop)) {
        LOGE("Get stack range error");
        return false;
    }
    LOGU("stackBottom: %" PRIx64 ", stackTop: %" PRIx64 "", (uint64_t)stackBottom, (uint64_t)stackTop);

    if (!withRegs) {
        regs_ = DfxRegs::Create();
        auto regsData = regs_->RawData();
        if (regsData == nullptr) {
            LOGE("params is nullptr");
            return false;
        }
        GetLocalRegs(regsData);
    }

    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs_;
    context.maps = maps_;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
    bool ret = Unwind(&context, maxFrameNum, skipFrameNum);
    return ret;
}

bool Unwinder::UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    if ((maps_ == nullptr) || (pid_ <= 0) || (tid < 0)) {
        LOGE("params is nullptr, pid: %d, tid: %d", pid_, tid);
        return false;
    }
    if (tid == 0) {
        tid = pid_;
    }
    LOGI("tid: %d", tid);
    if (!withRegs) {
        regs_ = DfxRegs::CreateRemoteRegs(tid);
    }
    if ((regs_ == nullptr)) {
        LOGE("regs is nullptr");
        return false;
    }

    UnwindContext context;
    context.pid = tid;
    context.regs = regs_;
    context.maps = maps_;
    bool ret = Unwind(&context, maxFrameNum, skipFrameNum);
    return ret;
}

#if defined(ENABLE_MIXSTACK)
bool Unwinder::StepArkJsFrame(uintptr_t& pc, uintptr_t& fp, uintptr_t& sp)
{
    const size_t JSFRAME_MAX = 64;
    JsFrame jsFrames[JSFRAME_MAX];
    size_t size = JSFRAME_MAX;
    if (memset_s(jsFrames, sizeof(JsFrame) * JSFRAME_MAX, 0, sizeof(JsFrame) * JSFRAME_MAX) != 0) {
        LOGE("Failed to memset_s jsFrames.");
        return false;
    }
    LOGI("+++ark pc: %" PRIx64 ", fp: %" PRIx64 ", sp: %" PRIx64 ".", (uint64_t)pc, (uint64_t)fp, (uint64_t)sp);
    if (DfxArk::GetArkNativeFrameInfo(pid_, pc, fp, sp, jsFrames, size) < 0) {
        return false;
    }
    LOGI("---ark pc: %" PRIx64 ", fp: %" PRIx64 ", sp: %" PRIx64 ", js frame size: %zu.",
        (uint64_t)pc, (uint64_t)fp, (uint64_t)sp, size);

    for (size_t i = 0; i < size; ++i) {
        DfxFrame frame;
        if (i == (size - 1)) {
            frame.pc = static_cast<uint64_t>(pc);
            frame.sp = static_cast<uint64_t>(sp);
            frame.fp = static_cast<uint64_t>(fp);
        }
        frame.isJsFrame = true;
        frame.index = frames_.size();
        frame.mapName = std::string(jsFrames[i].url);
        frame.funcName = std::string(jsFrames[i].functionName);
        frame.line = jsFrames[i].line;
        frame.column = jsFrames[i].column;
        AddFrame(frame);
    }
    return true;
}

bool Unwinder::StepArkJsFrame(uintptr_t& pc, uintptr_t& fp, uintptr_t& sp, bool& isJsFrame)
{
    LOGI("+++ark pc: %" PRIx64 ", fp: %" PRIx64 ", sp: %" PRIx64 ", isJsFrame: %d.",
        (uint64_t)pc, (uint64_t)fp, (uint64_t)sp, isJsFrame);
    if (DfxArk::StepArkFrame(memory_.get(), &(Unwinder::AccessMem), &fp, &sp, &pc, &isJsFrame) < 0) {
        LOGE("Failed to step ark Js frames, pc: %" PRIx64, (uint64_t)pc);
        return false;
    }
    LOGI("---ark pc: %" PRIx64 ", fp: %" PRIx64 ", sp: %" PRIx64 ", isJsFrame: %d.",
        (uint64_t)pc, (uint64_t)fp, (uint64_t)sp, isJsFrame);

    AddFrame(isJsFrame, pc, sp, fp);
    return true;
}
#endif

bool Unwinder::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        LOGE("params is nullptr?");
        return false;
    }
    Clear();

    bool needAdjustPc = false;
    size_t skipIndex = 0;
    uintptr_t prevPc = 0;
    uintptr_t prevSp = 0;
    do {
        if (skipIndex < skipFrameNum) {
            skipIndex++;
            continue;
        }
        if (frames_.size() == 0) {
            AddFrame(false, regs_->GetPc(), regs_->GetSp(), regs_->GetFp());
        }
        if (frames_.size() >= maxFrameNum) {
            LOGW("frames size: %zu", frames_.size());
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }

        DfxFrame& frame = frames_.back();
        if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
            if (needAdjustPc) {
                uintptr_t adjustPc = static_cast<uintptr_t>(frame.pc);
                DoPcAdjust(adjustPc);
                frame.pc = static_cast<uint64_t>(adjustPc);
            }
            needAdjustPc = true;
        }
        prevPc = static_cast<uintptr_t>(frame.pc);
        prevSp = static_cast<uintptr_t>(frame.sp);

        if (!Step(frame, ctx)) {
            break;
        }

        uintptr_t pc = regs_->GetPc();
        uintptr_t sp = regs_->GetSp();
        if (pc == prevPc && sp == prevSp) {
            if (pid_ >= 0) {
                MAYBE_UNUSED UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
                LOGU("pc and sp is same, tid: %d", uctx->pid);
            } else {
                LOGU("pc and sp is same");
            }
            lastErrorData_.SetAddrAndCode(pc, UNW_ERROR_REPEATED_FRAME);
            break;
        }
    } while (true);
    LOGU("Last error code: %d, addr: %p", (int)GetLastErrorCode(), reinterpret_cast<void *>(GetLastErrorAddr()));
    return (frames_.size() > 0);
}

bool Unwinder::UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if (regs_ == nullptr) {
        LOGE("params is nullptr?");
        return false;
    }
    pcs_.clear();

    size_t skipIndex = 0;
    uintptr_t pc = 0;
    uintptr_t fp = 0;
    do {
        if (skipIndex < skipFrameNum) {
            skipIndex++;
            continue;
        }
        if (pcs_.size() >= maxFrameNum) {
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }

        pc = regs_->GetPc();
        fp = regs_->GetFp();
        pcs_.emplace_back(pc);

        if (!FpStep(fp, pc, ctx) || (pc == 0)) {
            break;
        }
    } while (true);
    return (pcs_.size() > 0);
}

bool Unwinder::Step(uintptr_t& pc, uintptr_t& sp, void *ctx)
{
    DfxFrame frame;
    frame.pc = static_cast<uint64_t>(pc);
    frame.sp = static_cast<uint64_t>(sp);
    frame.fp = static_cast<uint64_t>(regs_->GetFp());
    bool ret = Step(frame, ctx);
    pc = regs_->GetPc();
    sp = regs_->GetSp();
    return ret;
}

bool Unwinder::Step(DfxFrame& frame, void *ctx)
{
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        LOGE("params is nullptr");
        return false;
    }
    uintptr_t pc = static_cast<uintptr_t>(frame.pc);
    uintptr_t sp = static_cast<uintptr_t>(frame.sp);
    MAYBE_UNUSED uintptr_t fp = static_cast<uintptr_t>(frame.fp);
    MAYBE_UNUSED bool isJsFrame = frame.isJsFrame;
    LOGU("+pc: %" PRIx64 ", sp: %" PRIx64 ", fp: %" PRIx64 "", (uint64_t)pc, (uint64_t)sp, (uint64_t)fp);

    std::shared_ptr<DfxMap> map = nullptr;
    MAYBE_UNUSED int mapRet = acc_->GetMapByPc(pc, map, ctx);
    if (mapRet != UNW_ERROR_NONE) {
        if (frames_.size() > 2) { //2, least 2 frame
            LOGE("Failed to get map, frames size: %zu", frames_.size());
            lastErrorData_.SetAddrAndCode(pc, mapRet);
            return false;
        }
    }
    frame.map = map;

#if defined(ENABLE_MIXSTACK)
#if defined(ONLINE_MIXSTACK)
    if (map != nullptr && map->IsArkExecutable()) {
        if (!StepArkJsFrame(pc, fp, sp)) {
            LOGE("Failed to step ark Js frames, pc: %" PRIx64, (uint64_t)pc);
            return false;
        }
#else
    if ((map != nullptr && map->IsArkExecutable()) || isJsFrame) {
        if (!StepArkJsFrame(pc, fp, sp, isJsFrame)) {
            LOGE("Failed to step ark Js frames, pc: %" PRIx64, (uint64_t)pc);
            return false;
        }
#endif
        regs_->SetPc(pc);
        regs_->SetSp(sp);
        regs_->SetFp(fp);
        return true;
    }
#endif

    bool ret = false;
    bool isSignalFrame = false;
    std::shared_ptr<RegLocState> rs = nullptr;
    do {
        if (isFpStep_) {
            if (enableFpCheckMapExec_ && (map != nullptr && !map->IsMapExec())) {
                LOGE("Fp step check map is not exec");
                return false;
            }
            break;
        }

        if (enableCache_) {
            // 1. find cache rs
            auto iter = rsCache_.find(pc);
            if (iter != rsCache_.end()) {
                if (pid_ != UNWIND_TYPE_CUSTOMIZE) {
                    LOGU("Find rs cache, pc: %p", reinterpret_cast<void *>(pc));
                }
                rs = iter->second;
                ret = true;
                break;
            }
        }

        // Check if this is a signal frame.
        if (regs_->StepIfSignalFrame(pc, memory_)) {
            isSignalFrame = true;
            break;
        }

        // 2. find unwind table and entry
        UnwindTableInfo uti;
        MAYBE_UNUSED int utiRet = acc_->FindUnwindTable(pc, uti, ctx);
        if (utiRet != UNW_ERROR_NONE) {
            lastErrorData_.SetAddrAndCode(pc, utiRet);
            LOGU("Failed to find unwind table ret: %d", utiRet);
            break;
        }

        // 3. parse instructions and get cache rs
        struct UnwindEntryInfo uei;
        rs = std::make_shared<RegLocState>();
#if defined(__arm__)
        if (!ret && uti.format == UNW_INFO_FORMAT_ARM_EXIDX) {
            if (!armExidx_->SearchEntry(pc, uti, uei)) {
                lastErrorData_.SetAddrAndCode(armExidx_->GetLastErrorAddr(), armExidx_->GetLastErrorCode());
                LOGE("Failed to search unwind entry?");
                break;
            }
            if (!armExidx_->Step((uintptr_t)uei.unwindInfo, rs)) {
                lastErrorData_.SetAddrAndCode(armExidx_->GetLastErrorAddr(), armExidx_->GetLastErrorCode());
                LOGU("Step exidx section error?");
            } else {
                ret = true;
            }
        }
#endif
        if (!ret && uti.format == UNW_INFO_FORMAT_REMOTE_TABLE) {
            if ((uti.isLinear == false && !dwarfSection_->SearchEntry(pc, uti, uei)) ||
                (uti.isLinear == true && !dwarfSection_->LinearSearchEntry(pc, uti, uei))) {
                lastErrorData_.SetAddrAndCode(dwarfSection_->GetLastErrorAddr(), dwarfSection_->GetLastErrorCode());
                LOGU("Failed to search unwind entry?");
                break;
            }
            memory_->SetDataOffset(uti.segbase);
            if (!dwarfSection_->Step((uintptr_t)uei.unwindInfo, regs_, rs)) {
                lastErrorData_.SetAddrAndCode(dwarfSection_->GetLastErrorAddr(), dwarfSection_->GetLastErrorCode());
                LOGU("Step dwarf section error?");
            } else {
                ret = true;
            }
        }

        if (ret && enableCache_) {
            // 4. update rs cache
            rsCache_.emplace(pc, rs);
            break;
        }
    } while (false);

    if (!isSignalFrame) {
        // 5. update regs and regs state
        if (ret) {
            SetLocalStackCheck(ctx, true);
            ret = Apply(regs_, rs);
        } else {
            if ((frames_.size() == 1) && enableLrFallback_ && regs_->SetPcFromReturnAddress(memory_)) {
                LOGW("Failed to step first frame, lr fallback");
                ret = true;
            }
        }
    } else {
        LOGW("Step signal frame, pc: %p", reinterpret_cast<void *>(pc));
        ret = true;
    }

#if defined(__aarch64__)
    if (!ret && enableFpFallback_) {
        ret = FpStep(fp, pc, ctx);
    }
#endif

    pc = regs_->GetPc();
    sp = regs_->GetSp();
    fp = regs_->GetFp();
    uintptr_t tmp = 0;
    if (ret && ((pc == 0) || (!memory_->ReadUptr(sp, &tmp, false)))) {
        LOGU("-pc: %" PRIx64 ", sp: %" PRIx64 ", fp: %" PRIx64 " error?", (uint64_t)pc, (uint64_t)sp, (uint64_t)fp);
        ret = false;
    } else {
        AddFrame(false, pc, sp, fp);
        LOGU("-pc: %" PRIx64 ", sp: %" PRIx64 ", fp: %" PRIx64 "", (uint64_t)pc, (uint64_t)sp, (uint64_t)fp);
    }
    return ret;
}

bool Unwinder::FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx)
{
#if defined(__aarch64__)
    LOGU("+fp: %lx, pc: %lx", (uint64_t)fp, (uint64_t)pc);
    if ((regs_ == nullptr) || (!CheckAndReset(ctx))) {
        LOGE("params is nullptr");
        return false;
    }
    if (!isFpStep_) {
        LOGI("First enter fp step, pc: %p", reinterpret_cast<void *>(pc));
        isFpStep_ = true;
    }

    uintptr_t prevFp = fp;
    uintptr_t ptr = fp;
    if (memory_->ReadUptr(ptr, &fp, true) &&
        memory_->ReadUptr(ptr, &pc, false)) {
        regs_->SetReg(REG_FP, &fp);
        regs_->SetReg(REG_PC, &pc);
        regs_->SetReg(REG_SP, &prevFp);
        if (pid_ == UNWIND_TYPE_CUSTOMIZE) {
            regs_->SetPc(StripPac(pc, pacMask_));
        }
        LOGU("-fp: %lx, pc: %lx", (uint64_t)fp, (uint64_t)pc);
        return true;
    }
#endif
    return false;
}

bool Unwinder::Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    bool ret = false;
    if (rs == nullptr || regs == nullptr) {
        return false;
    }
    ret = DfxInstructions::Apply(memory_, *(regs.get()), *(rs.get()));
    if (!ret) {
        LOGE("Failed to apply rs");
    }
#if defined(__arm__) || defined(__aarch64__)
    if (!rs->isPcSet) {
        regs_->SetReg(REG_PC, regs->GetReg(REG_LR));
    }
#endif

    if (rs->pseudoReg != 0) {
        regs->SetPc(StripPac(regs_->GetPc(), pacMask_));
    }
    return ret;
}

uintptr_t Unwinder::StripPac(uintptr_t inAddr, uintptr_t pacMask)
{
    uintptr_t outAddr = inAddr;
#if defined(__aarch64__)
    if (outAddr != 0) {
        if (pacMask != 0) {
            outAddr &= ~pacMask;
        } else {
            register uint64_t x30 __asm("x30") = inAddr;
            asm("hint 0x7" : "+r"(x30));
            outAddr = x30;
        }
        if (outAddr != inAddr) {
            LOGW("Strip pac in addr: %lx, out addr: %lx", (uint64_t)inAddr, (uint64_t)outAddr);
        }
    }
#endif
    return outAddr;
}

void Unwinder::DoPcAdjust(uintptr_t& pc)
{
    if (pc <= 0x4) {
        return;
    }
    uintptr_t sz = 0x4;
#if defined(__arm__)
    if (pc & 0x1) {
        uintptr_t val;
        if (pc < 0x5 || !(memory_->ReadMem(pc - 0x5, &val)) ||
            (val & 0xe000f000) != 0xe000f000) {
            sz = 0x2;
        }
    }
#elif defined(__x86_64__)
    sz = 0x1;
#endif
    pc -= sz;
}

std::vector<DfxFrame>& Unwinder::GetFrames()
{
    if (enableFillFrames_) {
        FillFrames(frames_);
    }
    return frames_;
}

void Unwinder::AddFrame(bool isJsFrame, uintptr_t pc, uintptr_t sp, uintptr_t fp)
{
    DfxFrame frame;
    frame.isJsFrame = isJsFrame;
    frame.index = frames_.size();
    frame.pc = static_cast<uint64_t>(pc);
    frame.sp = static_cast<uint64_t>(sp);
    frame.fp = static_cast<uint64_t>(fp);
    frames_.emplace_back(frame);
}

void Unwinder::AddFrame(DfxFrame& frame)
{
    frames_.emplace_back(frame);
}

void Unwinder::FillFrames(std::vector<DfxFrame>& frames)
{
    for (size_t i = 0; i < frames.size(); ++i) {
        auto& frame = frames[i];
        if (frame.isJsFrame) {
#if defined(OFFLINE_MIXSTACK)
            FillJsFrame(frame);
#endif
        } else {
            FillFrame(frame);
        }
    }
}

void Unwinder::FillFrame(DfxFrame& frame)
{
    if (frame.isJsFrame) {
        return;
    }
    if (frame.map == nullptr) {
        frame.relPc = frame.pc;
        frame.mapName = "Not mapped";
        LOGU("Current frame is not mapped.");
        return;
    }
    frame.relPc = frame.map->GetRelPc(frame.pc);
    frame.mapName = frame.map->GetElfName();
    frame.mapOffset = frame.map->offset;
    auto elf = frame.map->GetElf();
    if (elf == nullptr) {
        LOGU("elf is null, mapName: %s", frame.mapName.c_str());
        return;
    }
    LOGU("mapName: %s, mapOffset: %" PRIx64 "", frame.mapName.c_str(), frame.mapOffset);
    if (!DfxSymbols::GetFuncNameAndOffsetByPc(frame.relPc, elf, frame.funcName, frame.funcOffset)) {
        LOGU("Failed to get symbol, mapName: %s, relPc: %" PRIx64 "", frame.mapName.c_str(), frame.relPc);
    }
    frame.buildId = elf->GetBuildId();
}

void Unwinder::FillJsFrame(DfxFrame& frame)
{
    if (!frame.isJsFrame) {
        return;
    }
    if (frame.map == nullptr || frame.map->name.empty()) {
        LOGU("Current js frame is not mapped.");
        return;
    }
#if defined(OFFLINE_MIXSTACK)
    LOGU("Js frame pc: %" PRIx64 ", map name: %s", frame.pc, frame.map->name.c_str());
    std::shared_ptr<DfxExtractor> extractor = std::make_shared<DfxExtractor>(frame.map->name);
    uintptr_t loadOffset = 0;
    std::unique_ptr<uint8_t[]> dataPtr = nullptr;
    size_t dataSize = 0;
    if (!extractor->GetHapAbcInfo(loadOffset, dataPtr, dataSize)) {
        LOGW("Failed to get hap abc info: %s", frame.map->name.c_str());
        return;
    }
    JsFunction jsFunction;
    if (DfxArk::ParseArkFrameInfo(frame.pc, static_cast<uintptr_t>(frame.map->begin), loadOffset,
        dataPtr.get(), dataSize, &jsFunction) < 0) {
        LOGW("Failed to parse ark frame info: %s", frame.map->name.c_str());
        return;
    }
    frame.mapName = std::string(jsFunction.url);
    frame.funcName = std::string(jsFunction.functionName);
    frame.line = jsFunction.line;
    frame.column = jsFunction.column;
    LOGU("Js frame abc mapName: %s, funcName: %s, line: %d, column: %d",
        frame.mapName.c_str(), frame.funcName.c_str(), frame.line, frame.column);

    loadOffset = 0;
    dataPtr = nullptr;
    dataSize = 0;
    if (extractor->GetHapSourceMapInfo(loadOffset, dataPtr, dataSize)) {
        if (DfxArk::TranslateArkFrameInfo(dataPtr.get(), dataSize, &jsFunction) < 0) {
            LOGU("Failed to translate ark frame info: %s", frame.map->name.c_str());
        } else {
            frame.mapName = std::string(jsFunction.url);
            frame.funcName = std::string(jsFunction.functionName);
            frame.line = jsFunction.line;
            frame.column = jsFunction.column;
            LOGU("Js frame sourcemap mapName: %s, funcName: %s, line: %d, column: %d",
                frame.mapName.c_str(), frame.funcName.c_str(), frame.line, frame.column);
        }
    }
#endif
}

void Unwinder::GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs,
    std::shared_ptr<DfxMaps> maps)
{
    frames.clear();
    std::shared_ptr<DfxMap> map = nullptr;
    for (size_t i = 0; i < pcs.size(); ++i) {
        DfxFrame frame;
        frame.index = i;
        frame.pc = static_cast<uint64_t>(pcs[i]);
        if ((map != nullptr) && map->Contain(frame.pc)) {
            LOGU("map had matched");
        } else {
            if (!maps->FindMapByAddr(pcs[i], map) || (map == nullptr)) {
                LOGE("map is null");
                continue;
            }
        }
        frame.map = map;
        FillFrame(frame);
        frames.emplace_back(frame);
    }
}

bool Unwinder::GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, std::string& funcName, uint64_t& funcOffset)
{
    if (maps == nullptr) {
        return false;
    }
    std::shared_ptr<DfxMap> map = nullptr;
    if (!maps->FindMapByAddr(pc, map) || (map == nullptr)) {
        LOGE("Find map is null");
        return false;
    }
    uint64_t relPc = map->GetRelPc(static_cast<uint64_t>(pc));
    auto elf = map->GetElf();
    if (elf == nullptr) {
        LOGE("Get elf is null");
        return false;
    }
    return DfxSymbols::GetFuncNameAndOffsetByPc(relPc, elf, funcName, funcOffset);
}

std::string Unwinder::GetFramesStr(const std::vector<DfxFrame>& frames)
{
    return DfxFrameFormatter::GetFramesStr(frames);
}
} // namespace HiviewDFX
} // namespace OHOS
