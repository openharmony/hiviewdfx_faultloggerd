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

int DfxUnwindTable::ResetElfDynInfo(struct ElfDynInfo* edi)
{
    int ret = memset_s(edi, sizeof(ElfDynInfo), 0, sizeof(ElfDynInfo));
    edi->diCache.format = -1;
    edi->diDebug.format = -1;
#if defined(__arm__)
    edi->diArm.format = -1;
#endif
    return ret;
}

bool DfxUnwindTable::IsPcInUnwindInfo(struct UnwindDynInfo di, uintptr_t pc)
{
    if (di.format != -1 && pc >= di.startPc && pc < di.endPc) {
        return true;
    }
    return false;
}

int DfxUnwindTable::FindUnwindTable2(struct ElfDynInfo* edi, uintptr_t pc)
{
    if (IsPcInUnwindInfo(edi->diCache, pc) ||
#if defined(__arm__)
        IsPcInUnwindInfo(edi->diArm, pc) ||
#endif
        IsPcInUnwindInfo(edi->diDebug, pc)) {
        return UNW_ERROR_NONE;
    }

    DfxElf::DlCbData cbData;
    memset_s(&cbData, sizeof(cbData), 0, sizeof(cbData));
    cbData.pc = pc;
    ResetElfDynInfo(&cbData.edi);
    int ret = dl_iterate_phdr(DfxElf::DlPhdrCb, &cbData);
    if (ret == UNW_ERROR_NONE) {
        *edi = cbData.edi;
    }
    return ret;
}

int DfxUnwindTable::FindUnwindTable(struct ElfDynInfo* edi, uintptr_t pc, std::shared_ptr<DfxElf> elf)
{
    if (IsPcInUnwindInfo(edi->diCache, pc) ||
#if defined(__arm__)
        IsPcInUnwindInfo(edi->diArm, pc) ||
#endif
        IsPcInUnwindInfo(edi->diDebug, pc)) {
        return UNW_ERROR_NONE;
    }

    ResetElfDynInfo(edi);
    if (elf == nullptr || !elf->GetElfDynInfo(pc, edi)) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }

    // format != 1, but pc is not in elf
    if (edi->diCache.format != -1 && (pc < edi->diCache.startPc || pc >= edi->diCache.endPc)) {
        edi->diCache.format = -1;
    }
#if defined(__arm__)
    if (edi->diArm.format != -1 && (pc < edi->diArm.startPc || pc >= edi->diArm.endPc)) {
        edi->diArm.format = -1;
    }
#endif
    if (edi->diDebug.format != -1 && (pc < edi->diDebug.startPc || pc >= edi->diDebug.endPc)) {
        edi->diDebug.format = -1;
    }

    if (edi->diCache.format == -1 &&
#if defined(__arm__)
        edi->diArm.format == -1 &&
#endif
        edi->diDebug.format == -1) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }
    return UNW_ERROR_NONE;
}

int DfxUnwindTable::SearchUnwindTable(struct UnwindProcInfo* pi, struct UnwindDynInfo* di,\
    uintptr_t pc, DfxMemory* memory, bool needUnwindInfo)
{
#if defined(__arm__)
    if (di->format == UNW_INFO_FORMAT_ARM_EXIDX) {
        return ExdixSearchUnwindTable(pi, di, pc, memory, needUnwindInfo);
    }
#endif
    if (di->format == UNW_INFO_FORMAT_REMOTE_TABLE || di->format == UNW_INFO_FORMAT_IP_OFFSET) {
        return DwarfSearchUnwindTable(pi, di, pc, memory, needUnwindInfo);
    }
    return UNW_ERROR_NO_UNWIND_INFO;
}

int DfxUnwindTable::ExdixSearchUnwindTable(struct UnwindProcInfo* pi, struct UnwindDynInfo* di,\
    uintptr_t pc, DfxMemory* memory, bool needUnwindInfo)
{
    uintptr_t first = di->u.rti.tableData;
    uintptr_t last = di->u.rti.tableData + di->u.rti.tableLen - 8;
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
        if (!memory->ReadPrel31(last, &pi->startPc)) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }
        pi->endPc = di->endPc - 1;
    } else {
        while (first < last - 8) {
            entry = first + (((last - first) / 8 + 1) >> 1) * 8;
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
        if (!memory->ReadPrel31(cur, &pi->startPc)) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }

        cur = entry + 8;
        if (!memory->ReadPrel31(cur, &pi->endPc)) {
            return UNW_ERROR_ILLEGAL_VALUE;
        }

        pi->endPc--;
    }

    if (needUnwindInfo) {
        pi->unwindInfoSize = 8;
        pi->unwindInfo = (void *) entry;
        pi->format = UNW_INFO_FORMAT_ARM_EXIDX;
    }
    return UNW_ERROR_NONE;
}

int DfxUnwindTable::DwarfSearchUnwindTable(struct UnwindProcInfo* pi, struct UnwindDynInfo* di,\
    uintptr_t pc, DfxMemory* memory, bool needUnwindInfo)
{
    MAYBE_UNUSED auto segbase = di->u.rti.segbase;
    auto fdeCount = di->u.rti.tableLen;
    uintptr_t eh_frame_hdr_table = di->u.rti.tableData;
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
    MAYBE_UNUSED uintptr_t startPc = memory->Read<int32_t>(entry, true) + eh_frame_hdr;
    LOGU("entry: %llx, rel startPc: %llx, target relPc: %llx",
        (uint64_t)entry, (uint64_t)(startPc - segbase), (uint64_t)(pc - segbase));
    pi->unwindInfo = (void *) (memory->Read<int32_t>(entry, true) + eh_frame_hdr);
    LOGU("entry: %llx, fde entry: %llx", (uint64_t)entry, (uint64_t)pi->unwindInfo);
    pi->format = UNW_INFO_FORMAT_REMOTE_TABLE;
    return UNW_ERROR_NONE;
}
} // namespace HiviewDFX
} // namespace OHOS
