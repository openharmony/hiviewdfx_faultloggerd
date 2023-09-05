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
#include "arm_exidx.h"
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
        regs_ = DfxRegs::Create();
    }
    if (!regs_->GetRemoteRegs(pid_)) {
        return false;
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
    bool ret = false;
    lastErrorData_.addr = pc;
    DoPcAdjust(pc);
    memory_->SetCtx(ctx);
    auto iter = rsCache_.find(pc);
    if (iter != rsCache_.end()) {
        auto rs = iter->second;
        DfxInstructions instructions(memory_);
        ret = instructions.Apply(regs_, rs);
    }

    if (ret == false) {
        int errorCode = UNW_ERROR_NONE;
        UnwindDynInfo di;
        if ((errorCode = acc_->FindProcInfo(pc, &di, true, ctx)) != UNW_ERROR_NONE) {
            lastErrorData_.code = static_cast<uint16_t>(errorCode);
            return false;
        }

        struct UnwindProcInfo pi;
        if ((errorCode = DfxUnwindTable::SearchUnwindTable(&pi, &di, pc, memory_.get(), true)) != UNW_ERROR_NONE) {
            lastErrorData_.code = static_cast<uint16_t>(errorCode);
            return false;
        }

        auto rs = std::make_shared<RegLocState>();
        // we have get unwind info, then parser the exidx entry or ehframe fde
        if (pi.format == UNW_INFO_FORMAT_ARM_EXIDX) {
            ArmExidx armExidx(memory_);
            if (!armExidx.Eval((uintptr_t)pi.unwindInfo, regs_, rs)) {
                lastErrorData_.code = armExidx.GetLastErrorCode();
                lastErrorData_.addr = armExidx.GetLastErrorAddr();
                return false;
            }
        } else if (pi.format == UNW_INFO_FORMAT_REMOTE_TABLE) {
            // TODO: arm64 UNW_INFO_FORMAT_REMOTE_TABLE
            return false;
        }
        rsCache_.emplace(pc, rs);
    }

    pc = regs_->GetPc();
    sp = regs_->GetSp();
    LOGU("------pc: %llx, sp: %llx", (uint64_t)pc, (uint64_t)sp);
    return true;
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
