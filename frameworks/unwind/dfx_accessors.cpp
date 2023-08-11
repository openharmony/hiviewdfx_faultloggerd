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
#include <securec.h>
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
        *(uintptr_t *) addr = *val;
    } else {
        *val = *(uintptr_t *) addr;
    }
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::AccessReg(int reg, uintptr_t *val, int write, void *arg)
{
    UnwindLocalContext* ctx = reinterpret_cast<UnwindLocalContext *>(arg);
    if (reg > ctx->regsSize) {
        return UNW_ERROR_INVALID_REGS;
    }
    if (write) {
        *(uintptr_t *) ctx->regs[reg] = *val;
    } else {
        *val = *(uintptr_t *) ctx->regs[reg];
    }
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::FindProcInfo(uintptr_t pc, UnwindProcInfo *procInfo, int needUnwindInfo, void *arg)
{
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::ReadU8(uintptr_t* addr, uint8_t *val, void *arg)
{
    UnwindAlignedValue* ual = reinterpret_cast<UnwindAlignedValue *>(*addr);
    *val = ual->u8;
    *addr += sizeof(ual->u8);
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::ReadU16(uintptr_t* addr, uint16_t *val, void *arg)
{
    UnwindAlignedValue* ual = reinterpret_cast<UnwindAlignedValue *>(*addr);
    *val = ual->u16;
    *addr += sizeof(ual->u16);
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::ReadU32(uintptr_t* addr, uint32_t *val, void *arg)
{
    UnwindAlignedValue* ual = reinterpret_cast<UnwindAlignedValue *>(*addr);
    *val = ual->u32;
    *addr += sizeof(ual->u32);
    return UNW_ERROR_NONE;
}

int DfxAccessorsLocal::ReadU64(uintptr_t* addr, uint64_t *val, void *arg)
{
    UnwindAlignedValue* ual = reinterpret_cast<UnwindAlignedValue *>(*addr);
    *val = ual->u64;
    *addr += sizeof(ual->u64);
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::AccessMem(uintptr_t addr, uintptr_t *val, int write, void *arg)
{
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::AccessReg(int reg, uintptr_t *val, int write, void *arg)
{
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::FindProcInfo(uintptr_t pc, UnwindProcInfo *procInfo, int needUnwindInfo, void *arg)
{
    return UNW_ERROR_NONE;
}

int DfxAccessorsRemote::ReadU8(uintptr_t* addr, uint8_t *val, void *arg)
{
    uintptr_t valp, alignedAddr = *addr & (~sizeof(uintptr_t) + 1);
    uintptr_t off = *addr - alignedAddr;
    *addr += sizeof(uint8_t);
    int ret = AccessMem(alignedAddr, &valp, 0, arg);
#if UNWIND_BYTE_ORDER == UNWIND_LITTLE_ENDIAN
    valp >>= 8 * off;
#else
    valp >>= 8 * (sizeof(uintptr_t) - 1 - off);
#endif
    *val = static_cast<uint8_t>(valp);
    return ret;
}

int DfxAccessorsRemote::ReadU16(uintptr_t* addr, uint16_t *val, void *arg)
{
    int ret = UNW_ERROR_NONE;
    uint8_t v0, v1;
    if ((ret = ReadU8(addr, &v0, arg)) != UNW_ERROR_NONE
        || (ret = ReadU8(addr, &v1, arg)) != UNW_ERROR_NONE) {
        return ret;
    }
    if (bigEndian_ == UNWIND_LITTLE_ENDIAN) {
        *val = static_cast<uint16_t>(v1 << 8 | v0);
    } else {
        *val = static_cast<uint16_t>(v0 << 8 | v1);
    }
    return ret;
}

int DfxAccessorsRemote::ReadU32(uintptr_t* addr, uint32_t *val, void *arg)
{
    int ret = UNW_ERROR_NONE;
    uint16_t v0, v1;
    if ((ret = ReadU16(addr, &v0, arg)) != UNW_ERROR_NONE
        || (ret = ReadU16(addr, &v1, arg)) != UNW_ERROR_NONE) {
        return ret;
    }
    if (bigEndian_ == UNWIND_LITTLE_ENDIAN) {
        *val = static_cast<uint16_t>(v1 << 16 | v0);
    } else {
        *val = static_cast<uint16_t>(v0 << 16 | v1);
    }
    return ret;
}

int DfxAccessorsRemote::ReadU64(uintptr_t* addr, uint64_t *val, void *arg)
{
    int ret = UNW_ERROR_NONE;
    uint32_t v0, v1;
    if ((ret = ReadU32(addr, &v0, arg)) != UNW_ERROR_NONE
        || (ret = ReadU32(addr, &v1, arg)) != UNW_ERROR_NONE) {
        return ret;
    }
    if (bigEndian_ == UNWIND_LITTLE_ENDIAN) {
        *val = static_cast<uint16_t>(v1 << 32 | v0);
    } else {
        *val = static_cast<uint16_t>(v0 << 32 | v1);
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
