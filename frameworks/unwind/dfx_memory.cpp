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

#include "dfx_memory.h"
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
#define LOG_TAG "DfxMemory"
}

int DfxMemory::ReadReg(int reg, uintptr_t *val)
{
    return acc_->AccessReg(reg, val, 0, ctx_);
}

int DfxMemory::ReadMem(uintptr_t addr, uintptr_t *val)
{
    return acc_->AccessMem(addr, val, 0, ctx_);
}

int DfxMemory::Read(uintptr_t* addr, void* val, size_t size)
{
    int ret = UNW_ERROR_NONE;
    switch (size) {
        case 1: {
            uint8_t* valp = reinterpret_cast<uint8_t *>(val);
            ret = acc_->ReadU8(addr, valp, ctx_);
        }
            break;
        case 2: {
            uint16_t* valp = reinterpret_cast<uint16_t *>(val);
            ret = acc_->ReadU16(addr, valp, ctx_);
        }
            break;
        case 4: {
            uint32_t* valp = reinterpret_cast<uint32_t *>(val);
            ret = acc_->ReadU32(addr, valp, ctx_);
        }
            break;
        case 8: {
            uint64_t* valp = reinterpret_cast<uint64_t *>(val);
            ret = acc_->ReadU64(addr, valp, ctx_);
        }
            break;
        default:
            ret = UNW_ERROR_ILLEGAL_VALUE;
            break;
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
