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

#include <pthread.h>
#if defined(__arm__)
#include "arm_exidx.h"
#endif
#include "dwarf_section.h"
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_regs_get.h"
#include "dfx_log.h"
#include "dfx_instructions.h"
#include "dfx_unwind_table.h"
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
    frames_.clear();
    GetSelfStackRange(stackBottom_, stackTop_);

    if (pid_ == UWNIND_TYPE_LOCAL) {
        maps_ = DfxMaps::Create(getpid());
    } else {
        if (pid_ <= 0) {
            return;
        }
        maps_ = DfxMaps::Create(pid_);
    }
}

void Unwinder::Destroy()
{
    frames_.clear();
}

bool Unwinder::IsValidFrame(uintptr_t addr, uintptr_t stackTop, uintptr_t stackBottom)
{
    if (UNLIKELY(stackTop < stackBottom)) {
        return false;
    }
    return ((addr >= stackBottom) && (addr < stackTop - sizeof(uintptr_t)));
}

bool Unwinder::UnwindLocal(size_t maxFrameNum, size_t skipFrameNum)
{
    if (regs_ == nullptr) {
        regs_ = DfxRegs::Create();
    }

    UnwindLocalContext context;
    GetLocalRegs(regs_->RawData());
    context.regs = static_cast<uintptr_t *>(regs_->RawData());
    context.regsSize = regs_->RegsSize();

    return Unwind(&context, maxFrameNum, skipFrameNum);
}

bool Unwinder::UnwindRemote(size_t maxFrameNum, size_t skipFrameNum)
{
    if (regs_ == nullptr) {
        regs_ = DfxRegs::CreateRemoteRegs(pid_);
    }

    UnwindRemoteContext context;
    context.pid = pid_;
    context.regs = regs_;
    return Unwind(&context, maxFrameNum, skipFrameNum);
}

bool Unwinder::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    memory_->SetCtx(ctx);

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

        if (!memory_->ReadReg(REG_PC, &pc)) {
            LOGE("Read pc failed");
            lastErrorData_.code = UNW_ERROR_INVALID_REGS;
            break;
        }
        if (!memory_->ReadReg(REG_SP, &sp)) {
            LOGE("Read sp failed");
            lastErrorData_.code = UNW_ERROR_INVALID_REGS;
            break;
        }

        std::shared_ptr<DfxMap> map = nullptr;
        if (!maps_->FindMapByAddr(map, pc) || (map == nullptr)) {
            LOGE("map is null");
            lastErrorData_.code = pc;
            lastErrorData_.code = UNW_ERROR_INVALID_MAP;
            break;
        }

        elf_ = map->GetElf();
        if (elf_ == nullptr) {
            LOGE("elf is null");
            lastErrorData_.code = UNW_ERROR_INVALID_ELF;
            break;
        }

        if (pid_ > 0) {
            UnwindRemoteContext* context = reinterpret_cast<UnwindRemoteContext *>(ctx);
            context->elf = elf_;
        }
        uint64_t relPc = elf_->GetRelPc(pc, map->begin, map->end);
        stepPc = relPc;

        if (regs_->StepIfSignalHandler(relPc, elf_.get(), memory_.get())) {
            stepPc = relPc;
        } else if (Step(stepPc, sp, ctx) <= 0) {
            break;
        }

        index++;
    } while (true);
    return (curIndex > 0);
}

bool Unwinder::Step(uintptr_t& pc, uintptr_t& sp, void *ctx)
{
    LOGU("++++++pc: %llx, sp: %llx", (uint64_t)pc, (uint64_t)sp);
    lastErrorData_.addr = pc;
    int errorCode = UNW_ERROR_NONE;
    DoPcAdjust(pc);
    memory_->SetCtx(ctx);
    std::shared_ptr<RegLocState> rs = nullptr;
    bool ret = false;
    do {
        // 1. find cache rs
        auto iter = rsCache_.find(pc);
        if (iter != rsCache_.end()) {
            rs = iter->second;
            break;
        }

        // 2. find unwind table and entry
        UnwindTableInfo di;
        if ((errorCode = acc_->FindUnwindTable(pc, di, ctx)) != UNW_ERROR_NONE) {
            LOGE("Failed to find unwind table?");
            lastErrorData_.code = static_cast<uint16_t>(errorCode);
            break;
        }

        struct UnwindEntryInfo pi;
        if ((errorCode = DfxUnwindTable::SearchUnwindEntry(pi, di, pc, memory_)) != UNW_ERROR_NONE) {
            LOGE("Failed to search unwind entry?");
            lastErrorData_.code = static_cast<uint16_t>(errorCode);
            break;
        }

        // 3. parse instructions and get cache rs
        rs = std::make_shared<RegLocState>();
#if defined(__arm__)
        if (!ret && pi.format == UNW_INFO_FORMAT_ARM_EXIDX) {
            ArmExidx armExidx(memory_);
            if (!armExidx.Step((uintptr_t)pi.unwindInfo, regs_, rs)) {
                lastErrorData_.code = armExidx.GetLastErrorCode();
                lastErrorData_.addr = armExidx.GetLastErrorAddr();
            } else {
                ret = true;
            }
        }
#endif
        if (!ret && pi.format == UNW_INFO_FORMAT_REMOTE_TABLE) {
            DwarfSection dwarfSection(memory_);
            dwarfSection.SetDataOffset(di.segbase);
            if (!dwarfSection.Step((uintptr_t)pi.unwindInfo, regs_, rs)) {
                lastErrorData_.code = dwarfSection.GetLastErrorCode();
                lastErrorData_.addr = dwarfSection.GetLastErrorAddr();
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
    if (!Apply(regs_, rs)) {
        LOGE("Failed to Apply");
    }

    pc = regs_->GetPc();
    sp = regs_->GetSp();
    LOGU("------pc: %llx, sp: %llx", (uint64_t)pc, (uint64_t)sp);
    return ret;
}

bool Unwinder::Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    if (rs == nullptr || regs == nullptr) {
        return false;
    }
    if (DfxInstructions::Apply(*(regs.get()), memory_, *(rs.get()))) {
        if (!rs->isPcSet) {
            regs->SetReg(REG_PC, regs->GetReg(REG_LR));
        }
        return true;
    }
    return false;
}

void Unwinder::DoPcAdjust(uintptr_t& pc)
{
    if (pc <= 4) {
        return;
    }
    uintptr_t sz = 4;
#if defined(__arm__)
    if (pc & 1) {
        uintptr_t val;
        if (pc < 5 || !(memory_->ReadMem(pc - 5, &val)) ||
            (val & 0xe000f000) != 0xe000f000) {
            sz = 2;
        }
    }
#elif defined(__x86_64__)
    sz = 1;
#endif
    pc -= sz;
}
} // namespace HiviewDFX
} // namespace OHOS
