/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
    Clear();
    if (memset_s(&lastErrorData_, sizeof(UnwindErrorData), 0, sizeof(UnwindErrorData)) != 0) {
        LOGE("set memory of lastErrorData_ failed");
    }
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
    rsCache_.clear();
    pcs_.clear();
    frames_.clear();
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

bool Unwinder::UnwindLocal(size_t maxFrameNum, size_t skipFrameNum)
{
    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    if (!GetStackRange(stackBottom, stackTop)) {
        LOGE("Get stack range error");
        return false;
    }
    LOGU("stackBottom: %llx, stackTop: %llx", (uint64_t)stackBottom, (uint64_t)stackTop);

    regs_ = DfxRegs::Create();
    auto regsData = regs_->RawData();
    if (regsData == nullptr) {
        LOGE("params is nullptr");
        return false;
    }
    GetLocalRegs(regsData);

    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs_;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
    bool ret = Unwind(&context, maxFrameNum, skipFrameNum);
    return ret;
}

bool Unwinder::UnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    if ((pid_ <= 0) || (tid < 0)) {
        LOGE("params is nullptr, pid: %d", pid_);
        return false;
    }
    if (tid == 0) {
        tid = pid_;
    }
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
    bool ret = Unwind(&context, maxFrameNum, skipFrameNum);
    return ret;
}

bool Unwinder::GetMapByPc(uintptr_t pc, void *ctx, std::shared_ptr<DfxMap>& map)
{
    UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
    if (uctx->map != nullptr &&
        (pc >= (uintptr_t)uctx->map->begin && pc < (uintptr_t)uctx->map->end)) {
        LOGU("map had matched by ctx");
        map = uctx->map;
        return true;
    }

    for (size_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i].map != nullptr &&
            (pc >= (uintptr_t)frames_[i].map->begin && pc < (uintptr_t)frames_[i].map->end)) {
            LOGU("map had matched by frames");
            map = frames_[i].map;
            uctx->map = map;
            return true;
        }
    }

    if (!maps_->FindMapByAddr(pc, map) || (map == nullptr)) {
        return false;
    }
    uctx->map = map;
    return true;
}

bool Unwinder::IsMapExecByPc(uintptr_t pc, void *ctx)
{
    std::shared_ptr<DfxMap> map = nullptr;
    if (!GetMapByPc(pc, ctx, map)) {
        return false;
    }
    return map->IsValidName();
}

#if defined(ENABLE_MIXSTACK)
bool Unwinder::StepArkJsFrame(size_t& idx, size_t& curIdx)
{
    uintptr_t pc = regs_->GetPc();
    uintptr_t sp = regs_->GetSp();
    uintptr_t fp = regs_->GetFp();
    const size_t JSFRAME_MAX = 64;
    JsFrame jsFrames[JSFRAME_MAX];
    size_t size = JSFRAME_MAX;
    if (memset_s(jsFrames, sizeof(JsFrame) * JSFRAME_MAX, 0, sizeof(JsFrame) * JSFRAME_MAX) != 0) {
        LOGE("Failed to memset_s jsFrames.");
        return false;
    }
    LOGU("input ark pc: %llx, fp: %llx, sp: %llx.", (uint64_t)pc, (uint64_t)fp, (uint64_t)sp);
    int ret = DfxArk::GetArkNativeFrameInfo(pid_, pc, fp, sp, jsFrames, size);
    LOGU("output ark pc: %llx, fp: %llx, sp: %llx, js frame size: %d.", (uint64_t)pc, (uint64_t)fp, (uint64_t)sp, size);
    if (ret < 0) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        DfxFrame frame;
        frame.isJsFrame = true;
        frame.index = (curIdx++);
        frame.mapName = std::string(jsFrames[i].url);
        frame.funcName = std::string(jsFrames[i].functionName);
        frame.line = jsFrames[i].line;
        frame.column = jsFrames[i].column;
        frames_.push_back(frame);
        idx++;
    }
    regs_->SetPc(pc);
    regs_->SetSp(sp);
    regs_->SetFp(fp);
    return true;
}
#endif

bool Unwinder::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if ((ctx == nullptr) || (regs_ == nullptr) || (maps_ == nullptr)) {
        LOGE("params is nullptr?");
        return false;
    }
    pcs_.clear();
    frames_.clear();

    bool needAdjustPc = false;
    size_t index = 0;
    size_t curIndex = 0;
    uintptr_t pc = 0, sp = 0, stepPc = 0;
    uintptr_t prevPc = 0, prevSp = 0;

    std::shared_ptr<DfxMap> map = nullptr;
    pc = regs_->GetPc();
    if (!maps_->FindMapByAddr(pc, map) || (map == nullptr)) {
        regs_->SetPc(StripPac(pc, pacMask_));
    }

    do {
        if (index < skipFrameNum) {
            index++;
            continue;
        }
        curIndex = index - skipFrameNum;
        if (curIndex >= maxFrameNum) {
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }
        SetLocalStackCheck(ctx, false);

        pc = regs_->GetPc();
        sp = regs_->GetSp();
        if ((prevPc == pc) && (prevSp == sp)) {
            LOGE("pc and sp is same");
            break;
        }

        if (pid_ >= 0 || pid_ == UNWIND_TYPE_LOCAL) {
            if (!GetMapByPc(pc, ctx, map)) {
                if (curIndex != 0) {
                    lastErrorData_.SetAddrAndCode(pc, UNW_ERROR_INVALID_MAP);
                    break;
                }
                if (enableLrFallback_ && regs_->SetPcFromReturnAddress(memory_)) {
                    LOGW("Failed to get map, lr fallback");
                    continue;
                }
                uintptr_t fp = regs_->GetFp();
                if (!FpStep(fp, pc, ctx)) {
                    break;
                }
                LOGW("Failed to get map, fp fallback");
                continue;
            }
        }

        prevPc = pc;
        prevSp = sp;
        if (needAdjustPc) {
            DoPcAdjust(pc);
        }
        needAdjustPc = true;
#if defined(ENABLE_MIXSTACK)
        if (map->IsArkExecutable()) {
            if (!StepArkJsFrame(index, curIndex)) {
                LOGE("Failed to step ark Js frames.");
                break;
            }
            continue;
        }
#endif
        pcs_.push_back(pc);
        DfxFrame frame;
        frame.index = curIndex;
        frame.sp = static_cast<uint64_t>(sp);
        frame.pc = static_cast<uint64_t>(pc);
        frame.map = map;
        frames_.push_back(frame);

        stepPc = pc;
        if (!Step(stepPc, sp, ctx)) {
            break;
        }

        index++;
    } while (true);
    LOGU("Last error code: %d, addr: %p", (int)GetLastErrorCode(), (void *)GetLastErrorAddr());
    return (curIndex > 0);
}

bool Unwinder::UnwindByFp(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if (regs_ == nullptr) {
        LOGE("params is nullptr?");
        return false;
    }
    pcs_.clear();

    size_t idx = 0;
    size_t curIdx = 0;
    uintptr_t pc, fp;
    do {
        if (idx < skipFrameNum) {
            idx++;
            continue;
        }
        curIdx = idx - skipFrameNum;
        if (curIdx >= maxFrameNum) {
            lastErrorData_.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
            break;
        }

        pc = regs_->GetPc();
        fp = regs_->GetFp();
        pcs_.push_back(pc);

        if (!FpStep(fp, pc, ctx) || (pc == 0)) {
            break;
        }

        idx++;
    } while (true);
    return (curIdx > 0);
}

bool Unwinder::Step(uintptr_t& pc, uintptr_t& sp, void *ctx)
{
    if (regs_ == nullptr || memory_ == nullptr) {
        LOGE("params is nullptr");
        return false;
    }
    LOGU("++++++pc: %llx, sp: %llx", (uint64_t)pc, (uint64_t)sp);
    SetLocalStackCheck(ctx, false);
    memory_->SetCtx(ctx);
    bool isSignalFrame = false;

    std::shared_ptr<RegLocState> rs = nullptr;
    bool ret = false;
    do {
        if (enableCache_) {
            // 1. find cache rs
            auto iter = rsCache_.find(pc);
            if (iter != rsCache_.end()) {
                LOGU("Find rs cache, pc: %p", (void*)pc);
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
        MAYBE_UNUSED int utiRet = UNW_ERROR_NONE;
        if ((utiRet = acc_->FindUnwindTable(pc, uti, ctx)) != UNW_ERROR_NONE) {
            lastErrorData_.SetAddrAndCode(pc, UNW_ERROR_NO_UNWIND_INFO);
            LOGE("Failed to find unwind table ret: %d", utiRet);
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
        }
#if defined(__aarch64__)
        if (!ret && enableFpFallback_) {
            uintptr_t fp = regs_->GetFp();
            ret = FpStep(fp, pc, ctx);
        }
#endif
    } else {
        LOGW("Step signal frame");
        ret = true;
    }

    pc = regs_->GetPc();
    sp = regs_->GetSp();
    if (pc == 0) {
        ret = false;
    }
    LOGU("------pc: %llx, sp: %llx", (uint64_t)pc, (uint64_t)sp);
    return ret;
}

bool Unwinder::FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx)
{
#if defined(__aarch64__)
    LOGU("++++++fp: %llx, pc: %llx", (uint64_t)fp, (uint64_t)pc);
    SetLocalStackCheck(ctx, true);
    memory_->SetCtx(ctx);

    uintptr_t prevFp = fp;
    uintptr_t ptr = fp;
    if (memory_->ReadUptr(ptr, &fp, true) &&
        memory_->ReadUptr(ptr, &pc, false)) {
        if (enableFpCheckMapExec_ && (!IsMapExecByPc(pc, ctx))) {
            LOGE("Map is not exec");
            return false;
        }
        regs_->SetReg(REG_FP, &fp);
        regs_->SetReg(REG_PC, &pc);
        regs_->SetReg(REG_SP, &prevFp);
        LOGU("------fp: %llx, pc: %llx", (uint64_t)fp, (uint64_t)pc);
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
        LOGU("Pac addr: %llx", (uint64_t)outAddr);
        if (pacMask != 0) {
            outAddr &= ~pacMask;
        } else {
            register uint64_t x30 __asm("x30") = inAddr;
            asm("hint 0x7" : "+r"(x30));
            outAddr = x30;
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

void Unwinder::SetFrames(std::vector<DfxFrame>& frames)
{
    frames_ = frames;
}

void Unwinder::AddFrame(DfxFrame& frame)
{
    frames_.push_back(frame);
}

void Unwinder::FillFrames(std::vector<DfxFrame>& frames)
{
    for (size_t i = 0; i < frames.size(); ++i) {
        FillFrame(frames[i]);
    }
}

void Unwinder::FillFrame(DfxFrame& frame)
{
    if (frame.isJsFrame) {
        return;
    }
    if (frame.map == nullptr) {
        frame.mapName = "Not mapped";
        LOGE("Current frame is not mapped.");
        return;
    }
    frame.relPc = frame.map->GetRelPc(frame.pc);
    frame.mapName = frame.map->GetElfName();
    frame.mapOffset = frame.map->offset;
    LOGU("mapName: %s, mapOffset: %llx", frame.mapName.c_str(), frame.mapOffset);

    auto elf = frame.map->GetElf();
    if (elf == nullptr) {
        LOGE("elf is null");
        return;
    }
    DfxSymbols::GetFuncNameAndOffsetByPc(frame.relPc, elf, frame.funcName, frame.funcOffset);
    frame.buildId = elf->GetBuildId();
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
        frames.push_back(frame);
    }
}

std::string Unwinder::GetFramesStr(const std::vector<DfxFrame>& frames)
{
    return DfxFrameFormatter::GetFramesStr(frames);
}
} // namespace HiviewDFX
} // namespace OHOS
