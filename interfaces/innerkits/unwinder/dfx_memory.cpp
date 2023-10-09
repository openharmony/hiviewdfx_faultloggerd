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
#include "dwarf_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMemory"
}

bool DfxMemory::ReadReg(int regIdx, uintptr_t *val)
{
    if (acc_ != nullptr && acc_->AccessReg(regIdx, val, ctx_) == UNW_ERROR_NONE) {
        return true;
    }
    return false;
}

bool DfxMemory::ReadMem(uintptr_t addr, uintptr_t *val)
{
    if (acc_ != nullptr && acc_->AccessMem(addr, val, ctx_) == UNW_ERROR_NONE) {
        return true;
    }
    return false;
}

size_t DfxMemory::Read(uintptr_t& addr, void* val, size_t size, bool incre)
{
    if (val == nullptr) {
        return 0;
    }
    uintptr_t tmpAddr = addr;
    uint64_t maxSize;
    if (__builtin_add_overflow(tmpAddr, size, &maxSize)) {
        LOGE("size: %d", size);
        return 0;
    }

    size_t bytesRead = 0;
    uintptr_t tmpVal;
    if (alignAddr_ && (alignBytes_ != 0)) {
        size_t alignBytes = tmpAddr & (alignBytes_ - 1);
        if (alignBytes != 0) {
            uintptr_t alignedAddr = tmpAddr & (~alignBytes_ - 1);
            LOGU("alignBytes: %d, alignedAddr: %llx", alignBytes, static_cast<uint64_t>(alignedAddr));
            if (!ReadMem(alignedAddr, &tmpVal)) {
                return bytesRead;
            }
            uintptr_t valp = static_cast<uintptr_t>(tmpVal);
            size_t copyBytes = std::min(alignBytes_ - alignBytes, size);
            memcpy_s(val, copyBytes, reinterpret_cast<uint8_t*>(&valp) + alignBytes, copyBytes);
            tmpAddr += copyBytes;
            val = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(val) + copyBytes);
            size -= copyBytes;
            bytesRead += copyBytes;
        }
    }

    for (size_t i = 0; i < size / sizeof(uintptr_t); i++) {
        if (!ReadMem(tmpAddr, &tmpVal)) {
            return bytesRead;
        }
        memcpy_s(val, sizeof(uintptr_t), &tmpVal, sizeof(uintptr_t));
        val = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(val) + sizeof(uintptr_t));
        tmpAddr += sizeof(uintptr_t);
        bytesRead += sizeof(uintptr_t);
    }

    size_t leftOver = size & (sizeof(uintptr_t) - 1);
    if (leftOver) {
        if (!ReadMem(tmpAddr, &tmpVal)) {
            return bytesRead;
        }
        memcpy_s(val, leftOver, &tmpVal, leftOver);
        tmpAddr += leftOver;
        bytesRead += leftOver;
    }

    if (incre) {
        addr = tmpAddr;
    }
    return bytesRead;
}

bool DfxMemory::ReadU8(uintptr_t& addr, uint8_t *val, bool incre)
{
    if (Read(addr, val, sizeof(uint8_t), incre) == sizeof(uint8_t)) {
        return true;
    }
    return false;
}

bool DfxMemory::ReadU16(uintptr_t& addr, uint16_t *val, bool incre)
{
    if (Read(addr, val, sizeof(uint16_t), incre) == sizeof(uint16_t)) {
        return true;
    }
    return false;
}

bool DfxMemory::ReadU32(uintptr_t& addr, uint32_t *val, bool incre)
{
    if (Read(addr, val, sizeof(uint32_t), incre) == sizeof(uint32_t)) {
        return true;
    }
    return false;
}

bool DfxMemory::ReadU64(uintptr_t& addr, uint64_t *val, bool incre)
{
    if (Read(addr, val, sizeof(uint64_t), incre) == sizeof(uint64_t)) {
        return true;
    }
    return false;
}

bool DfxMemory::ReadUptr(uintptr_t& addr, uintptr_t *val, bool incre)
{
    if (Read(addr, val, sizeof(uintptr_t), incre) == sizeof(uintptr_t)) {
        return true;
    }
    return false;
}

bool DfxMemory::ReadString(uintptr_t& addr, std::string* str, size_t maxSize, bool incre)
{
    char buf[NAME_LEN];
    size_t size = 0;
    uintptr_t ptr = addr;
    for (size_t offset = 0; offset < maxSize; offset += size) {
        size_t readn = std::min(sizeof(buf), maxSize - offset);
        ptr = ptr + offset;
        size = Read(ptr, buf, readn, false);
        if (size == 0) {
            return false;
        }
        size_t length = strnlen(buf, size);
        if (length < size) {
            if (offset == 0) {
                str->assign(buf, length);
                return true;
            } else {
                str->assign(offset + length, '\0');
                Read(ptr, (void *)str->data(), str->size(), false);
                return true;
            }
        }
    }
    if (incre) {
        addr += str->size();
    }
    return false;
}

bool DfxMemory::ReadPrel31(uintptr_t& addr, uintptr_t *val)
{
    uintptr_t offset;
    if (!ReadUptr(addr, &offset, false)) {
        return false;
    }
    // int32_t signedData = static_cast<int32_t>(data << 1) >> 1;
    offset = static_cast<uintptr_t>(static_cast<int32_t>(offset << 1) >> 1);
    *val = addr + offset;
    return true;
}

uint64_t DfxMemory::ReadUleb128(uintptr_t& addr)
{
    uint64_t val = 0;
    uint64_t shift = 0;
    uint8_t byte;
    do {
        if (!ReadU8(addr, &byte, true)) {
            break;
        }

        val |= static_cast<uint64_t>(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return val;
}

int64_t DfxMemory::ReadSleb128(uintptr_t& addr)
{
    uint64_t val = 0;
    uint64_t shift = 0;
    uint8_t byte;
    do {
        if (!ReadU8(addr, &byte, true)) {
            break;
        }

        val |= static_cast<uint64_t>(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    if ((byte & 0x40) != 0) {
        val |= (-1ULL) << shift;
    }
    return static_cast<int64_t>(val);
}

size_t DfxMemory::GetEncodedSize(uint8_t encoding)
{
    switch (encoding & 0x0f) {
        case DW_EH_PE_absptr:
            return sizeof(uintptr_t);
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

uintptr_t DfxMemory::ReadEncodedValue(uintptr_t& addr, uint8_t encoding)
{
    uintptr_t startAddr = addr;
    uintptr_t val = 0;
    if (encoding == DW_EH_PE_omit) {
        return val;
    } else if (encoding == DW_EH_PE_aligned) {
        if (__builtin_add_overflow(addr, sizeof(uintptr_t) - 1, &addr)) {
            return val;
        }
        addr &= -sizeof(uintptr_t);
        ReadUptr(addr, &val, true);
        return val;
    }

    switch (encoding & DW_EH_PE_FORMAT_MASK) {
        case DW_EH_PE_absptr:
            ReadUptr(addr, &val, true);
            return val;
        case DW_EH_PE_uleb128:
            val = static_cast<uintptr_t>(ReadUleb128(addr));
            break;
        case DW_EH_PE_sleb128:
            val = static_cast<uintptr_t>(ReadSleb128(addr));
            break;
        case DW_EH_PE_udata1: {
            uint8_t tmp = 0;
            ReadU8(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        case DW_EH_PE_sdata1: {
            int8_t tmp = 0;
            ReadS8(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        case DW_EH_PE_udata2: {
            uint16_t tmp = 0;
            ReadU16(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        case DW_EH_PE_sdata2: {
            int16_t tmp = 0;
            ReadS16(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        case DW_EH_PE_udata4: {
            uint32_t tmp = 0;
            ReadU32(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        case DW_EH_PE_sdata4: {
            int32_t tmp = 0;
            ReadS32(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        case DW_EH_PE_udata8: {
            uint64_t tmp = 0;
            ReadU64(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        case DW_EH_PE_sdata8: {
            int64_t tmp = 0;
            ReadS64(addr, &tmp, true);
            val = static_cast<uintptr_t>(tmp);
        }
            break;
        default:
            LOGW("Unexpected encoding format 0x%x", encoding & DW_EH_PE_FORMAT_MASK);
            break;
    }

    switch (encoding & DW_EH_PE_APPL_MASK) {
        case DW_EH_PE_pcrel:
            val += startAddr;
            break;
        case DW_EH_PE_textrel:
            LOGE("XXX For now we don't support text-rel values");
            break;
        case DW_EH_PE_datarel:
            val += dataOffset_;
            break;
        case DW_EH_PE_funcrel:
            val += funcOffset_;
            break;
        default:
            break;
    }

    if (encoding & DW_EH_PE_indirect) {
        uintptr_t indirectAddr = val;
        ReadUptr(indirectAddr, &val, true);
    }
    return val;
}
} // namespace HiviewDFX
} // namespace OHOS
