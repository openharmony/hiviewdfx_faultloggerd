/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "dfx_accessors.h"
#include <algorithm>
#include <elf.h>
#include <securec.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_regs.h"
#include "dfx_elf.h"
#include "dfx_maps.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxAccessors"
}

bool DfxAccessorsLocal::IsValidFrame(uintptr_t addr, uintptr_t stackBottom, uintptr_t stackTop)
{
    if (UNLIKELY(stackTop < stackBottom)) {
        return false;
    }
    return ((addr >= stackBottom) && (addr < stackTop - sizeof(uintptr_t)));
}

int DfxAccessorsLocal::AccessMem(uintptr_t addr, uintptr_t *val, void *arg)
{
    UnwindLocalContext* ctx = reinterpret_cast<UnwindLocalContext *>(arg);
    if (ctx == nullptr) {
        return UNW_ERROR_INVALID_CONTEXT;
    }
    if (!IsValidFrame(addr, ctx->stackBottom, ctx->stackTop)) {
        LOGE("Failed to access addr: %llx", (uint64_t)addr);
        return UNW_ERROR_INVALID_MEMORY;
    }
    *val = *(uintptr_t *) addr;
    LOGD("val: %llx", *val);
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::AccessReg(int reg, uintptr_t *val, void *arg)
{
    UnwindLocalContext* ctx = reinterpret_cast<UnwindLocalContext *>(arg);
    if (ctx == nullptr) {
        return UNW_ERROR_INVALID_CONTEXT;
    }
    if (ctx->regs == nullptr || reg < 0 || reg >= (int)ctx->regs->RegsSize()) {
        return UNW_ERROR_INVALID_REGS;
    }

    *val = static_cast<uintptr_t>((*(ctx->regs))[reg]);
    LOGD("val: %llx", *val);
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::FindUnwindTable(uintptr_t pc, UnwindTableInfo& uti, void *arg)
{
    UnwindLocalContext *ctx = reinterpret_cast<UnwindLocalContext *>(arg);
    if (ctx == nullptr) {
        LOGE("ctx is null");
        return UNW_ERROR_INVALID_CONTEXT;
    }
    if (pc >= ctx->di.startPc && pc < ctx->di.endPc) {
        LOGU("FindUnwindTable had pc matched");
        uti = ctx->di;
        return UNW_ERROR_NONE;
    }
    int ret = DfxElf::FindUnwindTableLocal(uti, pc);
    if (ret == UNW_ERROR_NONE) {
        ctx->di = uti;
    }
    return ret;
}

int DfxAccessorsRemote::AccessMem(uintptr_t addr, uintptr_t *val, void *arg)
{
#if !defined(__LP64__)
    // Cannot read an address greater than 32 bits in a 32 bit context.
    if (addr > UINT32_MAX) {
        return UNW_ERROR_ILLEGAL_VALUE;
    }
#endif
    UnwindRemoteContext *ctx = reinterpret_cast<UnwindRemoteContext *>(arg);
    if (ctx == nullptr) {
        return UNW_ERROR_INVALID_CONTEXT;
    }
    pid_t pid = ctx->pid;
    int i, end;
    if (sizeof(long) == 4 && sizeof(uintptr_t) == 8) {
        end = 2;
    } else {
        end = 1;
    }

    uintptr_t tmpVal;
    for (i = 0; i < end; i++) {
        uintptr_t tmpAddr = ((i == 0) ? addr : addr + 4);
        errno = 0;

        tmpVal = (unsigned long) ptrace(PTRACE_PEEKDATA, pid, tmpAddr, nullptr);
        if (i == 0) {
            *val = 0;
        }

#if __BYTE_ORDER == __LITTLE_ENDIAN
        *val |= tmpVal << (i * 32);
#else
        *val |= (i == 0 && end == 2 ? tmpVal << 32 : tmpVal);
#endif
        if (errno) {
            LOGE("errno: %d", errno);
            return UNW_ERROR_ILLEGAL_VALUE;
        }
        LOGU("mem[%lx] -> %lx", (long) tmpAddr, (long) tmpVal);
    }
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::AccessReg(int reg, uintptr_t *val, void *arg)
{
    UnwindRemoteContext *ctx = reinterpret_cast<UnwindRemoteContext *>(arg);
    if (ctx == nullptr) {
        return UNW_ERROR_INVALID_CONTEXT;
    }
    if (ctx->regs == nullptr || reg < 0 || reg >= (int)ctx->regs->RegsSize()) {
        return UNW_ERROR_INVALID_REGS;
    }

    *val = static_cast<uintptr_t>((*(ctx->regs))[reg]);
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::FindUnwindTable(uintptr_t pc, UnwindTableInfo& uti, void *arg)
{
    UnwindRemoteContext *ctx = reinterpret_cast<UnwindRemoteContext *>(arg);
    if (ctx == nullptr || ctx->maps == nullptr) {
        return UNW_ERROR_INVALID_CONTEXT;
    }
    if (pc >= ctx->di.startPc && pc < ctx->di.endPc) {
        LOGU("FindUnwindTable had pc matched");
        uti = ctx->di;
        return UNW_ERROR_NONE;
    }
    if (ctx->map != nullptr && pc >= (uintptr_t)ctx->map->begin && pc < (uintptr_t)ctx->map->end) {
        LOGU("FindUnwindTable map had matched");
    } else {
        std::shared_ptr<DfxMap> map = nullptr;
        if (!ctx->maps->FindMapByAddr(map, pc) || (map == nullptr)) {
            LOGE("FindUnwindTable map is null");
            return UNW_ERROR_INVALID_MAP;
        }
        ctx->map = map;
    }
    auto elf = ctx->map->GetElf();
    if (elf == nullptr) {
        LOGE("FindUnwindTable elf is null");
        return UNW_ERROR_INVALID_ELF;
    }
    int ret = elf->FindUnwindTableInfo(uti, pc, ctx->map);
    if (ret == UNW_ERROR_NONE) {
        ctx->di = uti;
    }
    return ret;
}

int DfxAccessorsCustomize::AccessMem(uintptr_t addr, uintptr_t *val, void *arg)
{
    if (accessors_ == nullptr) {
        return -1;
    }
    return accessors_->AccessMem(addr, val, arg);
}

int DfxAccessorsCustomize::AccessReg(int reg, uintptr_t *val, void *arg)
{
    if (accessors_ == nullptr) {
        return -1;
    }
    return accessors_->AccessReg(reg, val, arg);
}

int DfxAccessorsCustomize::FindUnwindTable(uintptr_t pc, UnwindTableInfo& uti, void *arg)
{
    if (accessors_ == nullptr) {
        return -1;
    }
    return accessors_->FindUnwindTable(pc, uti, arg);
}
} // namespace HiviewDFX
} // namespace OHOS
