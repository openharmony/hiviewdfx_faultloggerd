/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "dfx_unwind_table.h"

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
#define LOG_TAG "DfxUnwindTable"
}

int DfxUnwindTable::IsPcInUnwindTable(struct UnwindTableInfo uti, uintptr_t pc)
{
    if (uti.format == -1) {
        return TABLE_ERROR_FORMAT;
    }
    // format != 1, but pc is not in unwind table
    if (pc < uti.startPc || pc >= uti.endPc) {
        return TABLE_ERROR_PC_NOT_IN;
    }
    return TABLE_ERROR_NONE;
}

int DfxUnwindTable::IsPcInElfTable(struct ElfTableInfo eti, uintptr_t pc)
{
    if (IsPcInUnwindTable(eti.diCache, pc) == TABLE_ERROR_FORMAT &&
#if defined(__arm__)
        IsPcInUnwindTable(eti.diArm, pc) == TABLE_ERROR_FORMAT &&
#endif
        IsPcInUnwindTable(eti.diDebug, pc) == TABLE_ERROR_FORMAT) {
        LOGE("Have not elf table info?");
        return UNW_ERROR_NO_UNWIND_INFO;
    }

    if (IsPcInUnwindTable(eti.diCache, pc) == TABLE_ERROR_NONE ||
#if defined(__arm__)
        IsPcInUnwindTable(eti.diArm, pc) == TABLE_ERROR_NONE ||
#endif
        IsPcInUnwindTable(eti.diDebug, pc) == TABLE_ERROR_NONE) {
        return UNW_ERROR_NONE;
    }
    LOGE("Pc(%p) is not in elf table info?", (void *)pc);
    return UNW_ERROR_PC_NOT_IN_UNWIND_INFO;
}

int DfxUnwindTable::FindUnwindTable2(struct ElfTableInfo& eti, uintptr_t pc)
{
    if (IsPcInElfTable(eti, pc) == TABLE_ERROR_NONE) {
        return UNW_ERROR_NONE;
    }

    DfxElf::DlCbData cbData;
    memset_s(&cbData, sizeof(cbData), 0, sizeof(cbData));
    cbData.pc = pc;
    DfxElf::ResetElfTableInfo(cbData.edi);
    (void)dl_iterate_phdr(DfxElf::DlPhdrCb, &cbData);

    int ret = IsPcInElfTable(cbData.edi, pc);
    if (ret == UNW_ERROR_NONE) {
        eti = cbData.edi;
    }
    return ret;
}

int DfxUnwindTable::FindUnwindTable(struct ElfTableInfo& eti, uintptr_t pc, std::shared_ptr<DfxElf> elf)
{
    if (IsPcInElfTable(eti, pc) == TABLE_ERROR_NONE) {
        return UNW_ERROR_NONE;
    }

    struct ElfTableInfo edi;
    DfxElf::ResetElfTableInfo(edi);
    if (elf == nullptr || !elf->GetElfTableInfo(pc, edi)) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }

    int ret = IsPcInElfTable(edi, pc);
    if (ret == TABLE_ERROR_NONE) {
        eti = edi;
    }
    return ret;
}

int DfxUnwindTable::SearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti,\
    uintptr_t pc, std::shared_ptr<DfxMemory> memory)
{
#if defined(__arm__)
    if (uti.format == UNW_INFO_FORMAT_ARM_EXIDX) {
        return ExdixSearchUnwindEntry(pi, uti, pc, memory);
    }
#endif
    if (uti.format == UNW_INFO_FORMAT_REMOTE_TABLE || uti.format == UNW_INFO_FORMAT_IP_OFFSET) {
        return DwarfSearchUnwindEntry(pi, uti, pc, memory);
    }
    return UNW_ERROR_NO_UNWIND_INFO;
}

int DfxUnwindTable::ExdixSearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti,\
    uintptr_t pc, std::shared_ptr<DfxMemory> memory)
{
    uintptr_t first = uti.tableData;
    uintptr_t last = uti.tableData + uti.tableLen - ARM_EXIDX_TABLE_SIZE;
    LOGU("ExdixSearchUnwindTable pc:%p, tableData: %llx", (void*)pc, (uint64_t)first);
    uintptr_t entry, val;

    if (!memory->ReadPrel31(first, &val) || pc < val) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }
    if (!memory->ReadPrel31(last, &val)) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }

    if (pc >= val) {
        entry = last;
        if (!memory->ReadPrel31(last, &(pi.startPc))) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }
        pi.endPc = uti.endPc - 1;
    } else {
        while (first < last - 8) {
            entry = first + (((last - first) / ARM_EXIDX_TABLE_SIZE + 1) >> 1) * ARM_EXIDX_TABLE_SIZE;
            if (!memory->ReadPrel31(entry, &val)) {
                return UNW_ERROR_ILLEGAL_VALUE;
            }
            if (pc < val) {
                last = entry;
            } else {
                first = entry;
            }
        }
        entry = first;

        uintptr_t cur = entry;
        if (!memory->ReadPrel31(cur, &(pi.startPc))) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }

        cur = entry + 8;
        if (!memory->ReadPrel31(cur, &(pi.endPc))) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }

        pi.endPc--;
    }

    pi.unwindInfoSize = ARM_EXIDX_TABLE_SIZE;
    pi.unwindInfo = (void *) entry;
    pi.format = UNW_INFO_FORMAT_ARM_EXIDX;
    return UNW_ERROR_NONE;
}

int DfxUnwindTable::DwarfSearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti,\
    uintptr_t pc, std::shared_ptr<DfxMemory> memory)
{
    MAYBE_UNUSED auto segbase = uti.segbase;
    auto fdeCount = uti.tableLen;
    uintptr_t eh_frame_hdr_table = uti.tableData;
    uintptr_t eh_frame_hdr = eh_frame_hdr_table - 12;
    LOGU("DwarfSearchUnwindTable pc:%p, segbase:%p, eh_frame_hdr_table:%p, tableLen: %d",
        (void*)pc, (void*)segbase, (void*)eh_frame_hdr_table, fdeCount);

    // do binary search, encode is stored in symbol file, we have no means to find?
    // hard code for 1b DwarfEncoding
    uintptr_t entry;
    uintptr_t low = 0;
    LOGU("target pc: %p, target relPc:%p", (void *)pc , (void *)(pc - segbase));
    for (uintptr_t len = fdeCount; len > 1;) {
        uintptr_t mid = low + (len / 2);
        entry = (uintptr_t) eh_frame_hdr_table + mid * 8;
        uintptr_t val = (uintptr_t)(memory->Read<int32_t>(entry, true));
        uintptr_t start = val + eh_frame_hdr; // todo read encode size from hdr?
        LOGU("target RelPc:%p, curRelPc:%p, idx:%d, val:%lx",
            (void *)(pc - segbase) , (void *)(start - segbase), (int)mid, (int64_t)val);

        if (start == pc) {
            low = mid;
            break;
        } else if (start < pc) {
            low = mid;
            len -= (len / 2);
        } else {
            len /= 2;
        }
    }

    entry = eh_frame_hdr_table + low * sizeof(uintptr_t);
    //MAYBE_UNUSED uintptr_t startPc = memory->Read<int32_t>(entry, true) + eh_frame_hdr;
    //LOGU("entry: %llx, rel startPc: %llx, target relPc: %llx",
    //    (uint64_t)entry, (uint64_t)(startPc - segbase), (uint64_t)(pc - segbase));
    entry += sizeof(int32_t);
    pi.unwindInfo = (void *) (memory->Read<int32_t>(entry, true) + eh_frame_hdr);
    LOGU("entry: %llx, fde entry: %llx", (uint64_t)entry, (uint64_t)pi.unwindInfo);
    pi.format = UNW_INFO_FORMAT_REMOTE_TABLE;
    return UNW_ERROR_NONE;
}
} // namespace HiviewDFX
} // namespace OHOS
