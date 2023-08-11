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

#include "dfx_memory_dwarf.h"
#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dwarf_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxDwarfMemory"
}

uintptr_t DfxDwarfMemory::ReadUintptr(uintptr_t* addr)
{
    return Read<uintptr_t>(addr, true);
}

uint64_t DfxDwarfMemory::ReadUleb128(uintptr_t* addr)
{
    uint64_t val = 0;
    uint64_t shift = 0;
    uint8_t byte;
    do {
        byte = Read<uint8_t>(addr, true);

        val |= static_cast<uint64_t>(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return val;
}

int64_t DfxDwarfMemory::ReadSleb128(uintptr_t* addr)
{
    uint64_t val = 0;
    uint64_t shift = 0;
    uint8_t byte;
    do {
        byte = Read<uint8_t>(addr, true);

        val |= static_cast<uint64_t>(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    if ((byte & 0x40) != 0) {
        val |= (-1ULL) << shift;
    }
    return static_cast<int64_t>(val);
}

template <typename AddressType>
size_t DfxDwarfMemory::GetEncodedSize(uint8_t encoding)
{
    switch (encoding & 0x0f) {
        case DW_EH_PE_absptr:
            return sizeof(AddressType);
        case DW_EH_PE_udata1:
        case DW_EH_PE_sdata1:
            return 1;
        case DW_EH_PE_udata2:
        case DW_EH_PE_sdata2:
            return 2;
        case DW_EH_PE_udata4:
        case DW_EH_PE_sdata4:
            return 4;
        case DW_EH_PE_udata8:
        case DW_EH_PE_sdata8:
            return 8;
        case DW_EH_PE_uleb128:
        case DW_EH_PE_sleb128:
        default:
            return 0;
    }
}

template <typename AddressType>
bool DfxDwarfMemory::ReadEncodedValue(uintptr_t* addr, uintptr_t* val, uint8_t encoding)
{
    if (encoding == DW_EH_PE_omit) {
        *val = 0;
        return true;
    }

    uintptr_t startAddr = *addr;
    switch (encoding & 0x0f) {
        case DW_EH_PE_absptr:
            if (sizeof(AddressType) != sizeof(uint64_t)) {
                *val = 0;
                return false;
            }
            *val = ReadUintptr(addr);
            break;
        case DW_EH_PE_uleb128:
            *val = static_cast<uintptr_t>(ReadUleb128(addr));
            break;
        case DW_EH_PE_sleb128:
            *val = static_cast<uintptr_t>(ReadSleb128(addr));
            break;
        case DW_EH_PE_udata1: {
            *val = static_cast<uintptr_t>(Read<uint8_t>(addr, true));
        }
            break;
        case DW_EH_PE_sdata1: {
            *val = static_cast<uintptr_t>(Read<int8_t>(addr, true));
        }
            break;
        case DW_EH_PE_udata2: {
            *val = static_cast<uintptr_t>(Read<uint16_t>(addr, true));
        }
            break;
        case DW_EH_PE_sdata2: {
            *val = static_cast<uintptr_t>(Read<int16_t>(addr, true));
        }
            break;
        case DW_EH_PE_udata4: {
            *val = static_cast<uintptr_t>(Read<uint32_t>(addr, true));
        }
            break;
        case DW_EH_PE_sdata4: {
            *val = static_cast<uintptr_t>(Read<int32_t>(addr, true));
        }
            break;
        case DW_EH_PE_udata8: {
            *val = static_cast<uintptr_t>(Read<uint64_t>(addr, true));
        }
            break;
        case DW_EH_PE_sdata8: {
            *val = static_cast<uintptr_t>(Read<int64_t>(addr, true));
        }
            break;
        default:
            return false;
    }

    switch (encoding) {
        case DW_EH_PE_absptr:
            break;
        case DW_EH_PE_pcrel:
            if (pcOffset_ != INT64_MAX) {
                *val += pcOffset_;
            }
            break;
        case DW_EH_PE_textrel:
            if (textOffset_ != UINT64_MAX) {
                *val += textOffset_;
            }
            break;
        case DW_EH_PE_datarel:
            if (dataOffset_ != UINT64_MAX) {
                *val += dataOffset_;
            }
            break;
        case DW_EH_PE_funcrel:
            if (funcOffset_ != UINT64_MAX) {
                *val += funcOffset_;
            }
            break;
        default:
            break;
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
