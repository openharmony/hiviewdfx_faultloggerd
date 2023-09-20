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
#include "dwarf_define.h"
#include "dfx_memory.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxUnwindTable"
}

int DfxUnwindTable::SearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti, uintptr_t pc)
{
#if defined(__arm__)
    if (uti.format == UNW_INFO_FORMAT_ARM_EXIDX) {
        return ExdixSearchUnwindEntry(pi, uti, pc);
    }
#endif
    if (uti.format == UNW_INFO_FORMAT_REMOTE_TABLE || uti.format == UNW_INFO_FORMAT_IP_OFFSET) {
        return DwarfSearchUnwindEntry(pi, uti, pc);
    }
    LOGE("UnwindTableInfo format: %d", uti.format);
    return UNW_ERROR_NO_UNWIND_INFO;
}

int DfxUnwindTable::ExdixSearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti, uintptr_t pc)
{
    uintptr_t first = uti.tableData;
    uintptr_t last = uti.tableData + uti.tableLen - ARM_EXIDX_TABLE_SIZE;
    LOGU("ExdixSearchUnwindTable pc:%p, tableData: %llx", (void*)pc, (uint64_t)first);
    uintptr_t entry, val;

    if (!DfxMemoryCpy::GetInstance().ReadPrel31(first, &val) || pc < val) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }
    if (!DfxMemoryCpy::GetInstance().ReadPrel31(last, &val)) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }

    if (pc >= val) {
        entry = last;
        if (!DfxMemoryCpy::GetInstance().ReadPrel31(last, &(pi.startPc))) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }
        pi.endPc = uti.endPc - 1;
    } else {
        while (first < last - 8) {
            entry = first + (((last - first) / ARM_EXIDX_TABLE_SIZE + 1) >> 1) * ARM_EXIDX_TABLE_SIZE;
            if (!DfxMemoryCpy::GetInstance().ReadPrel31(entry, &val)) {
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
        if (!DfxMemoryCpy::GetInstance().ReadPrel31(cur, &(pi.startPc))) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }

        cur = entry + 8;
        if (!DfxMemoryCpy::GetInstance().ReadPrel31(cur, &(pi.endPc))) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }

        pi.endPc--;
    }

    pi.gp = uti.gp;
    pi.unwindInfoSize = ARM_EXIDX_TABLE_SIZE;
    pi.unwindInfo = (void *) entry;
    pi.format = UNW_INFO_FORMAT_ARM_EXIDX;
    return UNW_ERROR_NONE;
}

int DfxUnwindTable::DwarfSearchUnwindEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti, uintptr_t pc)
{
    MAYBE_UNUSED auto segbase = uti.segbase;
    auto fdeCount = uti.tableLen;
    uintptr_t tableData = uti.tableData;
    LOGU("DwarfSearchUnwindTable segbase:%p, tableData:%p, tableLen: %d",
        (void*)segbase, (void*)tableData, fdeCount);

    // do binary search, encode is stored in symbol file, we have no means to find?
    // hard code for 1b DwarfEncoding
    uintptr_t entry;
    uintptr_t low = 0;
    DwarfTableEntry dwarfTableEntry;
    for (uintptr_t len = fdeCount; len > 1;) {
        uintptr_t cur = low + (len / 2);
        entry = (uintptr_t) tableData + cur * sizeof(DwarfTableEntry);
        LOGU("cur:%d, entry:%llx", (int)cur, (uint64_t)entry);
        DfxMemoryCpy::GetInstance().ReadS32(entry, &dwarfTableEntry.startPcOffset, true);
        uintptr_t startPc = static_cast<uintptr_t>(dwarfTableEntry.startPcOffset + segbase);
        LOGU("target Pc:%p, startPc:%p", (void *)pc, (void *)startPc);

        if (startPc == pc) {
            low = cur;
            break;
        } else if (startPc < pc) {
            low = cur;
            len -= (len / 2);
        } else {
            len /= 2;
        }
    }

    entry = (uintptr_t) tableData + low * sizeof(DwarfTableEntry);
    entry += 4;

    DfxMemoryCpy::GetInstance().ReadS32(entry, &dwarfTableEntry.fdeOffset, true);
    uintptr_t fdeAddr = static_cast<uintptr_t>(dwarfTableEntry.fdeOffset + segbase);
    pi.unwindInfo = (void *)(fdeAddr);
    LOGU("fde offset entry: %llx", (uint64_t)pi.unwindInfo);
    pi.format = UNW_INFO_FORMAT_REMOTE_TABLE;
    pi.gp = uti.gp;
    return UNW_ERROR_NONE;
}
} // namespace HiviewDFX
} // namespace OHOS
