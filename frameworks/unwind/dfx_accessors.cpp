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

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxAccessors"
}

int DfxAccessorsLocal::AccessMem(uintptr_t addr, uintptr_t *val, int write, void *arg)
{
    if (write) {
        LOGD("val: %llx", *val);
        *(uintptr_t *) addr = *val;
    } else {
        *val = *(uintptr_t *) addr;
        LOGD("val: %llx", *val);
    }
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::AccessReg(int reg, uintptr_t *val, int write, void *arg)
{
    UnwindLocalContext* ctx = reinterpret_cast<UnwindLocalContext *>(arg);
    if (ctx == nullptr || ctx->regs == nullptr) {
        LOGE("ctx is null");
        return UNW_ERROR_INVALID_CONTEXT;
    }
    if (reg < REG_EH || reg >= REG_LAST) {
        LOGE("Error reg: %d", reg);
        return UNW_ERROR_INVALID_REGS;
    }
    int idx = reg - REG_EH;
    if (write) {
        ctx->regs[idx] = *val;
    } else {
        *val = ctx->regs[idx];
    }
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::FindProcInfo(uintptr_t pc, UnwindProcInfo *procInfo, int needUnwindInfo, void *arg)
{
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::AccessMem(uintptr_t addr, uintptr_t *val, int write, void *arg)
{
    UnwindPtraceContext *ctx = reinterpret_cast<UnwindPtraceContext *>(arg);
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
        if (write) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            tmpVal = (i == 0 ? *val : *val >> 32);
#else
            tmpVal = (i == 0 && end == 2 ? *val >> 32 : *val);
#endif
            LOGU("mem[%lx] <- %lx\n", (long) tmpAddr, (long) tmpVal);
            ptrace(PTRACE_POKEDATA, pid, tmpAddr, tmpVal);
            if (errno) {
                return UNW_ERROR_ILLEGAL_VALUE;
            }
        } else {
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
                return UNW_ERROR_ILLEGAL_VALUE;
            }
            LOGU("mem[%lx] -> %lx\n", (long) tmpAddr, (long) tmpVal);
        }
    }
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::AccessReg(int reg, uintptr_t *val, int write, void *arg)
{
    UnwindPtraceContext *ctx = reinterpret_cast<UnwindPtraceContext *>(arg);
    if (ctx == nullptr) {
        return UNW_ERROR_INVALID_CONTEXT;
    }
    if (reg >= REG_LAST) {
        return UNW_ERROR_INVALID_REGS;
    }

    pid_t pid = ctx->pid;
    gregset_t regs;
    struct iovec iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov) == -1) {
        return UNW_ERROR_ILLEGAL_VALUE;
    }

    char *r = (char *)&regs + (reg * sizeof(uintptr_t));
    if (write) {
        memcpy_s(r, sizeof(uintptr_t), val, sizeof(uintptr_t));
        if (ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov) == -1) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }
    } else {
        memcpy_s(val, sizeof(uintptr_t), r, sizeof(uintptr_t));
    }
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::FindProcInfo(uintptr_t pc, UnwindProcInfo *procInfo, int needUnwindInfo, void *arg)
{
    return UNW_ERROR_NONE;
}
} // namespace HiviewDFX
} // namespace OHOS
