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
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_regs_get.h"
#include "dfx_log.h"
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
}

void Unwinder::Destroy()
{
    if (memory_ != nullptr) {
        delete memory_;
        memory_ = nullptr;
    }
    if (acc_ != nullptr) {
        delete acc_;
        acc_ = nullptr;
    }
    frames_.clear();
}


bool Unwinder::IsValidFrame(uintptr_t frame, uintptr_t stackTop, uintptr_t stackBottom)
{
    if (UNLIKELY(stackTop < stackBottom)) {
        return false;
    }
    return ((frame >= stackBottom) && (frame < stackTop - sizeof(uintptr_t)));
}

bool Unwinder::Unwind(void *ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if (regs_ == nullptr) {
        regs_ = DfxRegs::Create();
    }

    if (memory_ == nullptr) {
        if (pid_ == UWNIND_TYPE_CUSTOMIZE) {
            if (ctx == nullptr) {
                return false;
            }
            memory_ = new DfxMemory(acc_, ctx);
        } else if (pid_ == UWNIND_TYPE_LOCAL) {
            if (ctx == nullptr) {
                UnwindLocalContext context;
                GetLocalRegs(regs_->RawData());
                context.regs = static_cast<uintptr_t *>(regs_->RawData());
                context.regsSize = regs_->RegsSize();
                memory_ = new DfxMemory(acc_, &context);
                maps_ = DfxMaps::Create(getpid());
            } else {
                UnwindLocalContext* context = reinterpret_cast<UnwindLocalContext *>(ctx);
                regs_->SetRegsData(context->regs);
                memory_ = new DfxMemory(acc_, ctx);
            }
        } else {
            if (pid_ <= 0) {
                return false;
            }
            UnwindRemoteContext context;
            context.pid = pid_;
            memory_ = new DfxMemory(acc_, &context);
            maps_ = DfxMaps::Create(pid_);
        }
    }

    if (memory_ == nullptr) {
        return false;
    }

    size_t index = 0;
    size_t curIndex = 0;
    uintptr_t pc, sp;
    do {
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFrameNum) {
            index++;
            continue;
        }
        curIndex = index - skipFrameNum;
        if (curIndex < maxFrameNum) {
            break;
        }

        if (!memory_->ReadReg(REG_PC, &pc)) {
            LOGE("Read pc failed");
            break;
        }
        if (!memory_->ReadReg(REG_SP, &sp)) {
            LOGE("Read sp failed");
            break;
        }

        std::shared_ptr<DfxMap> map = nullptr;
        if (!maps_->FindMapByAddr(map, pc) || (map == nullptr)) {
            LOGE("map is null");
            break;
        }

        elf_ = map->GetElf();
        if (elf_ == nullptr) {
            LOGE("elf is null");
            break;
        }
        uint64_t relPc = elf_->GetRelPc(pc, map->begin, map->end);
        uint64_t stepPc = relPc;
        uint64_t pcAdjustment = elf_->GetPcAdjustment(relPc);
        stepPc -= pcAdjustment;

        if (regs_->StepIfSignalHandler(relPc, elf_.get(), memory_)) {
            stepPc = relPc;
        } else if (Step(stepPc) <= 0) {
            break;
        }

        index++;
    } while (true);
    return (curIndex > 0);
}

int Unwinder::Step(uintptr_t pc)
{
    return 0;
}

bool Unwinder::FindExidxEntry(uintptr_t pc, uintptr_t& entryOffset)
{
    ShdrInfo shdr;
    if (elf_ == nullptr || !elf_->GetArmExdixInfo(shdr)) {
        return false;
    }

    static std::unordered_map<size_t, uintptr_t> addrs;
    uintptr_t exidxOffset = static_cast<uintptr_t>(shdr.offset);
    size_t exidxTotalEntries = static_cast<size_t>(shdr.size / 8);
    size_t first = 0;
    size_t last = exidxTotalEntries;
    while (first < last) {
        size_t current = (first + last) / 2;
        uintptr_t addr = addrs[current];
        if (addr == 0) {
            uintptr_t curOffset = exidxOffset + current * 8;
            if (!memory_->ReadPrel31(&curOffset, &addr)) {
                lastErrorData_.addr = curOffset;
                return false;
            }
            addrs[current] = addr;
        }

        if (pc == addr) {
            entryOffset = exidxOffset + current * 8;
            return true;
        }
        if (pc < addr) {
            last = current;
        } else {
            first = current + 1;
        }
    }
    if (last != 0) {
        entryOffset = exidxOffset + (last - 1) * 8;
        return true;
    }
    lastErrorData_.addr = pc;
    lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS
