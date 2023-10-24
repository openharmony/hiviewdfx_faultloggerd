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

#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_frame_formatter.h"
#include "dfx_instructions.h"
#include "dfx_log.h"
#include "dfx_regs_get.h"
#include "dfx_symbols.h"
#include "stack_util.h"
#include "string_printf.h"

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
    memory_ = std::make_shared<DfxMemory>(acc_);
    lastErrorData_.code = UNW_ERROR_NONE;
    lastErrorData_.addr = 0;
    rsCache_.clear();
    pcs_.clear();
    frames_.clear();
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

bool Unwinder::UnwindRemote(size_t maxFrameNum, size_t skipFrameNum)
{
    regs_ = DfxRegs::CreateRemoteRegs(pid_);
    if ((regs_ == nullptr) || (pid_ <= 0)) {
        LOGE("params is nullptr, pid: %d", pid_);
        return false;
    }

    UnwindContext context;
    context.pid = pid_;
    context.regs = regs_;
    bool ret = Unwind(&context, maxFrameNum, skipFrameNum);
    return ret;
}

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
    uintptr_t pc, sp, stepPc;
    do {
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFrameNum) {
            index++;
            continue;
        }
        curIndex = index - skipFrameNum;
        if (curIndex >= maxFrameNum) {
            lastErrorData_.code = UNW_ERROR_MAX_FRAMES_EXCEEDED;
            break;
        }

        pc = regs_->GetPc();
        sp = regs_->GetSp();
        pcs_.push_back(pc);

        DfxFrame frame;
        std::shared_ptr<DfxMap> map = nullptr;
        if (pid_ >= 0 || pid_ == UNWIND_TYPE_LOCAL) {
            UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
            if (uctx->map != nullptr && pc >= (uintptr_t)uctx->map->begin && pc < (uintptr_t)uctx->map->end) {
                LOGU("map had matched");
                map = uctx->map;
            } else {
                if (!maps_->FindMapByAddr(map, pc) || (map == nullptr)) {
                    LOGE("map is null");
                    lastErrorData_.code = UNW_ERROR_INVALID_MAP;
                    break;
                }
                uctx->map = map;
            }
        }
        frame.index = curIndex;
        frame.pc = static_cast<uint64_t>(pc);
        frame.map = map;
        frames_.push_back(frame);

        stepPc = pc;
        if (needAdjustPc) {
            DoPcAdjust(stepPc);
        }
        needAdjustPc = true;

        if (!Step(stepPc, sp, ctx)) {
            break;
        }

        index++;
    } while (true);
    return (curIndex > 0);
}

bool Unwinder::Step(uintptr_t& pc, uintptr_t& sp, void *ctx)
{
    if (regs_ == nullptr || memory_ == nullptr) {
        LOGE("params is nullptr");
        return false;
    }
    LOGU("++++++pc: %llx, sp: %llx", (uint64_t)pc, (uint64_t)sp);
    lastErrorData_.addr = pc;
    if (pid_ == UNWIND_TYPE_LOCAL) {
        UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
        uctx->stackCheck = false;
    }
    memory_->SetCtx(ctx);

    // Check if this is a signal frame.
    if (regs_->StepIfSignalFrame(pc, memory_)) {
        pc = regs_->GetPc();
        sp = regs_->GetSp();
        LOGW("Step signal frame, pc: %llx, sp: %llx", (uint64_t)pc, (uint64_t)sp);
        return true;
    }

    std::shared_ptr<RegLocState> rs = nullptr;
    bool ret = false;
    do {
        // 1. find cache rs
        auto iter = rsCache_.find(pc);
        if (iter != rsCache_.end()) {
            LOGU("Find rs cache, pc: %p", (void*)pc);
            rs = iter->second;
            ret = true;
            break;
        }

        // 2. find unwind table and entry
        UnwindTableInfo uti;
        if ((lastErrorData_.code = acc_->FindUnwindTable(pc, uti, ctx)) != UNW_ERROR_NONE) {
            LOGE("Failed to find unwind table? errorCode: %d", lastErrorData_.code);
            break;
        }

        // 3. parse instructions and get cache rs
        struct UnwindEntryInfo uei;
        rs = std::make_shared<RegLocState>();
#if defined(__arm__)
        if (!ret && uti.format == UNW_INFO_FORMAT_ARM_EXIDX) {
            if (!armExidx_->SearchEntry(pc, uti, uei)) {
                lastErrorData_.code = armExidx_->GetLastErrorCode();
                LOGE("Failed to search unwind entry? errorCode: %d", lastErrorData_.code);
                break;
            }
            if (!armExidx_->Step((uintptr_t)uei.unwindInfo, rs)) {
                lastErrorData_.code = armExidx_->GetLastErrorCode();
                lastErrorData_.addr = armExidx_->GetLastErrorAddr();
                LOGU("Step exidx section error, errorCode: %d", lastErrorData_.code);
            } else {
                ret = true;
            }
        }
#endif
        if (!ret && uti.format == UNW_INFO_FORMAT_REMOTE_TABLE) {
            if ((uti.isLinear == false && !dwarfSection_->SearchEntry(pc, uti, uei)) ||
                (uti.isLinear == true && !dwarfSection_->LinearSearchEntry(pc, uti, uei))) {
                lastErrorData_.code = dwarfSection_->GetLastErrorCode();
                LOGE("Failed to search unwind entry? errorCode: %d", lastErrorData_.code);
                break;
            }
            memory_->SetDataOffset(uti.segbase);
            if (!dwarfSection_->Step((uintptr_t)uei.unwindInfo, regs_, rs)) {
                lastErrorData_.code = dwarfSection_->GetLastErrorCode();
                lastErrorData_.addr = dwarfSection_->GetLastErrorAddr();
                LOGU("Step dwarf section error, errorCode: %d", lastErrorData_.code);
            } else {
                ret = true;
            }
        }

        if (ret) {
            // 4. update rs cache
            rsCache_.emplace(pc, rs);
            break;
        }
    } while (false);

    // 5. update regs and regs state
    if (ret) {
        if (pid_ == UNWIND_TYPE_LOCAL) {
            UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
            uctx->stackCheck = true;
        }
        ret = Apply(regs_, rs);
    }

    if (!ret && (pid_ == UNWIND_TYPE_LOCAL)) {
        uintptr_t fp = regs_->GetFp();
        ret = FpStep(fp, pc, ctx);
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
    UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
    LOGU("++++++fp: %llx, pc: %llx", (uint64_t)fp, (uint64_t)pc);
    uintptr_t prevFp = fp;
    if (DfxAccessorsLocal::IsValidFrame(prevFp, uctx->stackBottom, uctx->stackTop)) {
        fp = *reinterpret_cast<uintptr_t*>(prevFp);
        pc = *reinterpret_cast<uintptr_t*>(prevFp + sizeof(uintptr_t));
        regs_->SetReg(REG_FP, &fp);
        regs_->SetReg(REG_PC, &pc);
        LOGU("------fp: %llx, pc: %llx", (uint64_t)fp, (uint64_t)pc);
        return true;
    }
    return false;
}

bool Unwinder::Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    bool ret = false;
    if (rs == nullptr || regs == nullptr) {
        return false;
    }
    ret = DfxInstructions::Apply(*(regs.get()), memory_, *(rs.get()));
    if (!ret) {
        LOGE("Failed to apply rs");
    }
    if (!rs->isPcSet) {
        regs->SetReg(REG_PC, regs->GetReg(REG_LR));
    }
    return ret;
}

void Unwinder::FillFrames(std::vector<DfxFrame>& frames)
{
    for (size_t i = 0; i < frames.size(); ++i) {
        DfxFrame& frame = frames[i];
        FillFrame(frame);
    }
}

void Unwinder::FillFrame(DfxFrame& frame)
{
    if (frame.map == nullptr) {
        LOGE("map is null");
        return;
    }
    frame.mapName = frame.map->name;
    frame.mapOffset = frame.map->offset;
    frame.relPc = frame.map->GetRelPc(frame.pc);
    LOGU("relPc: %llx, mapName: %s", frame.relPc, frame.mapName.c_str());

    auto elf = frame.map->GetElf();
    if (elf == nullptr) {
        LOGE("elf is null");
        return;
    }
    DfxSymbols::GetFuncNameAndOffsetByPc(frame.relPc, elf, frame.funcName, frame.funcOffset);
    frame.buildId = elf->GetBuildId();
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

void Unwinder::GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs,
    std::shared_ptr<DfxMaps> maps)
{
    frames.clear();
    std::shared_ptr<DfxMap> map = nullptr;
    for (size_t i = 0; i < pcs.size(); ++i) {
        DfxFrame frame;
        frame.index = i;
        uintptr_t pc = static_cast<uint64_t>(pcs[i]);
        if ((map != nullptr) && map->Contain(static_cast<uint64_t>(pc))) {
            LOGU("map had matched");
        } else {
            if (!maps->FindMapByAddr(map, pc) || (map == nullptr)) {
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
