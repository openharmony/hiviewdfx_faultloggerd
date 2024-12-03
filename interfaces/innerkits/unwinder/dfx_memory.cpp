/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#if is_ohos && !is_mingw
#include <sys/uio.h>
#endif
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_log.h"
#include "dwarf_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMemory"

static const int SEVEN_BIT_OFFSET = 7;
static const int TWO_BYTE_SIZE = 2;
static const int FOUR_BYTE_SIZE = 4;
static const int EIGHT_BYTE_SIZE = 8;
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
    uintptr_t tmpAddr = addr;
    uint64_t maxSize;
    if (val == nullptr || __builtin_add_overflow(tmpAddr, size, &maxSize)) {
        DFXLOGE("val is nullptr or size(%{public}zu) overflow", size);
        return 0;
    }
    size_t bytesRead = 0;
    uintptr_t tmpVal;
    if (alignAddr_ && (alignBytes_ != 0)) {
        size_t alignBytes = tmpAddr & (static_cast<size_t>(alignBytes_) - 1);
        if (alignBytes != 0) {
            uintptr_t alignedAddr = tmpAddr & (~(static_cast<uintptr_t>(alignBytes_)) - 1);
            DFXLOGU("alignBytes: %{public}zu, alignedAddr: %{public}" PRIx64 "", alignBytes,
                static_cast<uint64_t>(alignedAddr));
            if (!ReadMem(alignedAddr, &tmpVal)) {
                return bytesRead;
            }
            uintptr_t valp = static_cast<uintptr_t>(tmpVal);
            size_t copyBytes = std::min(static_cast<size_t>(alignBytes_) - alignBytes, size);
            if (memcpy_s(val, copyBytes, reinterpret_cast<uint8_t*>(&valp) + alignBytes, copyBytes) != 0) {
                return bytesRead;
            }
            tmpAddr += copyBytes;
            val = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(val) + copyBytes);
            size -= copyBytes;
            bytesRead += copyBytes;
        }
    }
    for (size_t i = 0; i < size / sizeof(uintptr_t); i++) {
        if (!ReadMem(tmpAddr, &tmpVal) || memcpy_s(val, sizeof(uintptr_t), &tmpVal, sizeof(uintptr_t)) != 0) {
            return bytesRead;
        }
        val = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(val) + sizeof(uintptr_t));
        tmpAddr += sizeof(uintptr_t);
        bytesRead += sizeof(uintptr_t);
    }
    size_t leftOver = size & (sizeof(uintptr_t) - 1);
    if (leftOver) {
        if (!ReadMem(tmpAddr, &tmpVal) || memcpy_s(val, leftOver, &tmpVal, leftOver) != 0) {
            return bytesRead;
        }
        tmpAddr += leftOver;
        bytesRead += leftOver;
    }
    if (incre) {
        addr = tmpAddr;
    }
    return bytesRead;
}

bool DfxMemory::ReadString(uintptr_t& addr, std::string* str, size_t maxSize, bool incre)
{
    if (str == nullptr) {
        return false;
    }
    char buf[NAME_BUF_LEN];
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
                Read(addr, (void*)str->data(), str->size(), false);
                return true;
            }
        }
    }
    if (incre && str != nullptr) {
        addr += str->size();
    }
    return false;
}

bool DfxMemory::ReadPrel31(uintptr_t& addr, uintptr_t *val)
{
    uintptr_t offset;
    if (!Read<uintptr_t>(addr, &offset, false)) {
        return false;
    }
    offset = static_cast<uintptr_t>(static_cast<int32_t>(offset << 1) >> 1);
    *val = addr + offset;
    return true;
}

uint64_t DfxMemory::ReadUleb128(uintptr_t& addr)
{
    uint64_t val = 0;
    uint64_t shift = 0;
    uint8_t u8 = 0;
    do {
        if (!Read<uint8_t>(addr, &u8, true)) {
            break;
        }

        val |= static_cast<uint64_t>(u8 & 0x7f) << shift;
        shift += SEVEN_BIT_OFFSET;
    } while (u8 & 0x80);
    return val;
}

int64_t DfxMemory::ReadSleb128(uintptr_t& addr)
{
    uint64_t val = 0;
    uint64_t shift = 0;
    uint8_t byte = 0;
    do {
        if (!Read<uint8_t>(addr, &byte, true)) {
            break;
        }

        val |= static_cast<uint64_t>(byte & 0x7f) << shift;
        shift += SEVEN_BIT_OFFSET;
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
            return TWO_BYTE_SIZE;
        case DW_EH_PE_udata4:
        case DW_EH_PE_sdata4:
            return FOUR_BYTE_SIZE;
        case DW_EH_PE_udata8:
        case DW_EH_PE_sdata8:
            return EIGHT_BYTE_SIZE;
        case DW_EH_PE_uleb128:
        case DW_EH_PE_sleb128:
        default:
            return 0;
    }
}

void DfxMemory::ReadFormatEncodedValue(uintptr_t &addr, uintptr_t &val, uint8_t formatEncoding)
{
    switch (formatEncoding) {
        case DW_EH_PE_uleb128:
            val = static_cast<uintptr_t>(ReadUleb128(addr));
            break;
        case DW_EH_PE_sleb128:
            val = static_cast<uintptr_t>(ReadSleb128(addr));
            break;
        case DW_EH_PE_udata1:
            val = static_cast<uintptr_t>(ReadValue<uint8_t>(addr, true));
            break;
        case DW_EH_PE_sdata1:
            val = static_cast<uintptr_t>(ReadValue<int8_t>(addr, true));
            break;
        case DW_EH_PE_udata2:
            val = static_cast<uintptr_t>(ReadValue<uint16_t>(addr, true));
            break;
        case DW_EH_PE_sdata2:
            val = static_cast<uintptr_t>(ReadValue<int16_t>(addr, true));
            break;
        case DW_EH_PE_udata4:
            val = static_cast<uintptr_t>(ReadValue<uint32_t>(addr, true));
            break;
        case DW_EH_PE_sdata4:
            val = static_cast<uintptr_t>(ReadValue<int32_t>(addr, true));
            break;
        case DW_EH_PE_udata8:
            val = static_cast<uintptr_t>(ReadValue<uint64_t>(addr, true));
            break;
        case DW_EH_PE_sdata8:
            val = static_cast<uintptr_t>(ReadValue<int64_t>(addr, true));
            break;
        default:
            DFXLOGW("Unexpected encoding format 0x%{public}x", formatEncoding);
            break;
    }
}

uintptr_t DfxMemory::ReadEncodedValue(uintptr_t &addr, uint8_t encoding)
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
        Read<uintptr_t>(addr, &val, true);
        return val;
    }

    uint8_t formatEncoding = encoding & DW_EH_PE_FORMAT_MASK;
    if (formatEncoding == DW_EH_PE_absptr) {
        Read<uintptr_t>(addr, &val, true);
        return val;
    } else {
        ReadFormatEncodedValue(addr, val, formatEncoding);
    }

    switch (encoding & DW_EH_PE_APPL_MASK) {
        case DW_EH_PE_pcrel:
            val += startAddr;
            break;
        case DW_EH_PE_textrel:
            DFXLOGE("XXX For now we don't support text-rel values");
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
        Read<uintptr_t>(indirectAddr, &val, true);
    }
    return val;
}
#if is_ohos && !is_mingw
size_t DfxMemory::ReadProcMemByPid(const pid_t pid, const uint64_t addr, void* data, size_t size)
{
    constexpr size_t maxSize = 64;
    struct iovec RemoteIovs[maxSize];

    uint64_t cur = addr;
    size_t totalRead = 0;
    struct iovec dataIov = {
        .iov_base = &reinterpret_cast<uint8_t*>(data)[totalRead],
        .iov_len = size,
    };
    size_t iovecsIndex = 0;
    while (size > 0) {
        if (cur >= UINTPTR_MAX) {
            return totalRead;
        }
        RemoteIovs[iovecsIndex].iov_base = reinterpret_cast<void*>(cur);
        uintptr_t misalign = cur & static_cast<uint64_t>(getpagesize() - 1);
        size_t iovLen = std::min(getpagesize() - misalign, size);

        size -= iovLen;
        if (__builtin_add_overflow(cur, iovLen, &cur)) {
            return totalRead;
        }

        RemoteIovs[iovecsIndex].iov_len = iovLen;
        ++iovecsIndex;
        if (iovecsIndex >= maxSize || size <= 0) {
            ssize_t count = process_vm_readv(pid, &dataIov, 1, RemoteIovs, iovecsIndex, 0);
            if (count == -1) {
                return totalRead;
            }
            totalRead += static_cast<size_t>(count);
            if (iovecsIndex >= maxSize) {
                iovecsIndex -= maxSize;
            }
            dataIov.iov_base = &reinterpret_cast<uint8_t*>(data)[totalRead];
            dataIov.iov_len = size;
        }
    }

    return totalRead;
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
