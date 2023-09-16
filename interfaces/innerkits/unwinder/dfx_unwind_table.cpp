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
    if (IsPcInUnwindTable(eti.diEhHdr, pc) == TABLE_ERROR_FORMAT &&
#if defined(__arm__)
        IsPcInUnwindTable(eti.diExidx, pc) == TABLE_ERROR_FORMAT &&
#endif
        IsPcInUnwindTable(eti.diDebug, pc) == TABLE_ERROR_FORMAT) {
        LOGE("Have not elf table info?");
        return UNW_ERROR_NO_UNWIND_INFO;
    }

    if (IsPcInUnwindTable(eti.diEhHdr, pc) == TABLE_ERROR_NONE ||
#if defined(__arm__)
        IsPcInUnwindTable(eti.diExidx, pc) == TABLE_ERROR_NONE ||
#endif
        IsPcInUnwindTable(eti.diDebug, pc) == TABLE_ERROR_NONE) {
        return UNW_ERROR_NONE;
    }
    LOGE("Pc(%p) is not in elf table info?", (void *)pc);
    return UNW_ERROR_PC_NOT_IN_UNWIND_INFO;
}

int DfxUnwindTable::ResetElfTableInfo(struct ElfTableInfo& edi)
{
    int ret = memset_s(&edi, sizeof(ElfTableInfo), 0, sizeof(ElfTableInfo));
    edi.diEhHdr.format = -1;
    edi.diDebug.format = -1;
#if defined(__arm__)
    edi.diExidx.format = -1;
#endif
    return ret;
}

bool DfxUnwindTable::GetExidxTableInfo(struct UnwindTableInfo& ti,
    std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf)
{
    if ((map == nullptr) || (elf == nullptr)) {
        return false;
    }
#if defined(__arm__)
    uintptr_t loadBase = elf->GetLoadBase(map->begin, map->offset);

    ShdrInfo shdr;
    if (elf->GetSectionInfo(shdr, ARM_EXIDX)) {
        ti.startPc = elf->GetStartPc();
        ti.endPc = elf->GetEndPc();
        ti.gp = 0;
        ti.format = UNW_INFO_FORMAT_ARM_EXIDX;
        ti.tableData = loadBase + shdr.addr;
        ti.tableLen = shdr.size;
        LOGU("Exidx tableLen: %d, tableData: %llx", (int)ti.tableLen, (uint64_t)ti.tableData);
        return true;
    }
#endif
    return false;
}

bool DfxUnwindTable::GetEhHdrTableInfo(struct UnwindTableInfo& ti,
    std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf)
{
    if ((map == nullptr) || (elf == nullptr)) {
        return false;
    }
    uintptr_t loadBase = elf->GetLoadBase(map->begin, map->offset);

    ShdrInfo shdr;
    if (elf->GetSectionInfo(shdr, EH_FRAME_HDR)) {
        ti.startPc = elf->GetStartPc();
        ti.endPc = elf->GetEndPc();
        ti.gp = elf->GetGlobalPointer();
        LOGU("Elf mmap ptr: %p", (void*)elf->GetMmapPtr());
        struct DwarfEhFrameHdr* hdr = (struct DwarfEhFrameHdr *) (shdr.offset + (char *) elf->GetMmapPtr());
        if (hdr->version != DW_EH_VERSION) {
            LOGE("Hdr version(%d) error", hdr->version);
            return false;
        }

        auto acc = std::make_shared<DfxAccessorsLocal>(false);
        auto memory = std::make_shared<DfxMemory>(acc);
        size_t tableEntrySize = memory->GetEncodedSize(hdr->tableEnc);
        if (tableEntrySize == 0) {
            LOGE("Hdr get table encode size error, hdr->tableEnc: %x", hdr->tableEnc);
            return false;
        }
        memory->SetDataOffset(ti.gp);
        LOGU("Eh hdr gp: %llx, offset: %llx", (uint64_t)ti.gp, (uint64_t)shdr.offset);

        uintptr_t ptr = (uintptr_t)(&(hdr->ehFrame));
        LOGU("hdr: %llx, ehFrame: %llx", (uint64_t)hdr, (uint64_t)ptr);
        LOGU("ehFramePtrEnc: %x, fdeCountEnc: %x", hdr->ehFramePtrEnc, hdr->fdeCountEnc);
        MAYBE_UNUSED uintptr_t ehFrameStart = memory->ReadEncodedValue(ptr, hdr->ehFramePtrEnc);
        uintptr_t fdeCount = memory->ReadEncodedValue(ptr, hdr->fdeCountEnc);
        if (fdeCount == 0) {
            LOGE("Hdr no FDEs?");
            return false;
        }
        LOGU("ehFrameStart: %llx, fdeCount: %llx", (uint64_t)ehFrameStart, (uint64_t)fdeCount);

        ti.format = UNW_INFO_FORMAT_REMOTE_TABLE;
        ti.namePtr = 0;
        ti.tableLen = (fdeCount * sizeof(DwarfTableEntry)) / sizeof(uintptr_t);
        ti.tableData = ((loadBase + shdr.addr) + (ptr - (uintptr_t)hdr));
        ti.segbase = loadBase + shdr.addr;
        LOGU("EhHdr tableLen: %d, tableData: %llx, segbase: %llx",
            (int)ti.tableLen, (uint64_t)ti.tableData, (uint64_t)ti.segbase);
        return true;
    }
    return false;
}

bool DfxUnwindTable::GetElfTableInfo(struct ElfTableInfo& eti, uintptr_t pc,
    std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf)
{
    if (elf == nullptr || !elf->IsValid()) {
         return false;
    }
    ResetElfTableInfo(eti);

    bool hasTableInfo = false;
#if defined(__arm__)
    hasTableInfo = GetExidxTableInfo(eti.diExidx, map, elf);
#endif
    if (!hasTableInfo) {
        hasTableInfo = GetEhHdrTableInfo(eti.diEhHdr, map, elf);
    }

    if (hasTableInfo) {
        eti.startPc = elf->GetStartPc();
        eti.endPc = elf->GetEndPc();
        return true;
    }
    return false;
}

int DfxUnwindTable::DlPhdrCb(struct dl_phdr_info *info, size_t size, void *data)
{
    struct DlCbData *cbData = (struct DlCbData *)data;
    ElfTableInfo *edi = &cbData->edi;
    const ElfW(Phdr) *pText = nullptr;
    const ElfW(Phdr) *pDynamic = nullptr;
#if defined(__arm__)
    const ElfW(Phdr) *pArmExidx = nullptr;
#endif
    const ElfW(Phdr) *pEhHdr = nullptr;
    struct DwarfEhFrameHdr *hdr = nullptr;
    struct DwarfEhFrameHdr synthHdr;
    const ElfW(Phdr) *phdr = info->dlpi_phdr;
    ElfW(Addr) loadBase = info->dlpi_addr, maxLoadAddr = 0;
    for (size_t i = 0; i < info->dlpi_phnum; i++, phdr++) {
        switch (phdr->p_type) {
        case PT_LOAD: {
            ElfW(Addr) vaddr = phdr->p_vaddr + loadBase;
            if (cbData->pc >= vaddr && cbData->pc < vaddr + phdr->p_memsz) {
                pText = phdr;
            }

            if (vaddr + phdr->p_filesz > maxLoadAddr) {
                maxLoadAddr = vaddr + phdr->p_filesz;
            }
            break;
        }
#if defined(__arm__)
        case PT_ARM_EXIDX: {
            pArmExidx = phdr;
            break;
        }
#endif
        case PT_GNU_EH_FRAME: {
            pEhHdr = phdr;
            break;
        }
        case PT_DYNAMIC: {
            pDynamic = phdr;
            break;
        }
        default:
            break;
        }
    }
    if (pText == nullptr) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }
    uintptr_t startPc = pText->p_vaddr + loadBase;
    uintptr_t endPc = startPc + pText->p_memsz;

    bool hasTableInfo = false;
#if defined(__arm__)
    if (pArmExidx) {
        edi->diExidx.format = UNW_INFO_FORMAT_ARM_EXIDX;
        edi->diExidx.startPc = startPc;
        edi->diExidx.endPc = endPc;
        edi->diExidx.gp = 0;
        edi->diExidx.namePtr = (uintptr_t) info->dlpi_name;
        edi->diExidx.tableData = pArmExidx->p_vaddr + loadBase;
        edi->diExidx.tableLen = pArmExidx->p_memsz;
        LOGU("Exidx tableLen: %d, tableData: %llx", (int)edi->diExidx.tableLen, (uint64_t)edi->diExidx.tableData);
        hasTableInfo = true;
    }
#endif

    if (pEhHdr) {
        hdr = (struct DwarfEhFrameHdr *) (pEhHdr->p_vaddr + loadBase);
    } else {
        LOGW("No .eh_frame_hdr section found");
        ElfW(Addr) ehFrame = FindSection(info, EH_FRAME);
        if (ehFrame != 0) {
            LOGD("using synthetic .eh_frame_hdr section for %s", info->dlpi_name);
            synthHdr.version = DW_EH_VERSION;
            synthHdr.ehFramePtrEnc = DW_EH_PE_absptr |
                ((sizeof(ElfW(Addr)) == 4) ? DW_EH_PE_udata4 : DW_EH_PE_udata8);
            synthHdr.fdeCountEnc = DW_EH_PE_omit;
            synthHdr.tableEnc = DW_EH_PE_omit;
            synthHdr.ehFrame = ehFrame;
            hdr = &synthHdr;
        }
    }
    if (hdr != nullptr) {
        if (pDynamic) {
            ElfW(Dyn) *dyn = (ElfW(Dyn) *)(pDynamic->p_vaddr + loadBase);
            for (; dyn->d_tag != DT_NULL; ++dyn) {
                if (dyn->d_tag == DT_PLTGOT) {
                    edi->diEhHdr.gp = dyn->d_un.d_ptr;
                    break;
                }
            }
        } else {
            edi->diEhHdr.gp = 0;
        }

        if (hdr->version != DW_EH_VERSION) {
            LOGE("Hdr version(%d) error", hdr->version);
            return false;
        }

        auto acc = std::make_shared<DfxAccessorsLocal>();
        auto memory = std::make_shared<DfxMemory>(acc);

        uintptr_t ptr = (uintptr_t)(&(hdr->ehFrame));
        LOGU("hdr: %llx, ehFrame: %llx", (uint64_t)hdr, (uint64_t)ptr);
        memory->SetDataOffset(edi->diEhHdr.gp);
        MAYBE_UNUSED uintptr_t ehFrameStart = memory->ReadEncodedValue(ptr, hdr->ehFramePtrEnc);
        uintptr_t fdeCount = memory->ReadEncodedValue(ptr, hdr->fdeCountEnc);
        LOGU("ehFrameStart: %llx, fdeCount: %llx", (uint64_t)ehFrameStart, (uint64_t)fdeCount);

        edi->diEhHdr.startPc = startPc;
        edi->diEhHdr.endPc = endPc;
        edi->diEhHdr.format = UNW_INFO_FORMAT_REMOTE_TABLE;
        edi->diEhHdr.namePtr = (uintptr_t) info->dlpi_name;
        edi->diEhHdr.tableLen = (fdeCount * sizeof(DwarfTableEntry)) / sizeof(uintptr_t);
        edi->diEhHdr.tableData = ptr;
        edi->diEhHdr.segbase = (uintptr_t)hdr;
        LOGU("EhHdr tableLen: %d, tableData: %llx, segbase: %llx",
            (int)edi->diEhHdr.tableLen, (uint64_t)edi->diEhHdr.tableData, (uint64_t)edi->diEhHdr.segbase);
    }

    if (hasTableInfo) {
        cbData->edi.startPc = startPc;
        cbData->edi.endPc = endPc;
        return UNW_ERROR_NONE;
    }
    return UNW_ERROR_NO_UNWIND_INFO;
}

ElfW(Addr) DfxUnwindTable::FindSection(struct dl_phdr_info *info, const std::string secName)
{
    const char *file = info->dlpi_name;
    if (strlen(file) == 0) {
        file = PROC_SELF_EXE_PATH;
    }

    auto elf = DfxElf::Create(file);
    if (elf == nullptr) {
        return 0;
    }

    ElfW(Addr) addr = 0;
    ShdrInfo shdr;
    if (!elf->GetSectionInfo(shdr, secName)) {
        return 0;
    }
    addr = shdr.addr + info->dlpi_addr;
    return addr;
}


int DfxUnwindTable::FindUnwindTableLocal(struct ElfTableInfo& eti, uintptr_t pc)
{
    if (IsPcInElfTable(eti, pc) == TABLE_ERROR_NONE) {
        return UNW_ERROR_NONE;
    }

    DlCbData cbData;
    memset_s(&cbData, sizeof(cbData), 0, sizeof(cbData));
    cbData.pc = pc;
    ResetElfTableInfo(cbData.edi);
    (void)dl_iterate_phdr(DlPhdrCb, &cbData);

    int ret = IsPcInElfTable(cbData.edi, pc);
    if (ret == UNW_ERROR_NONE) {
        eti = cbData.edi;
    }
    return ret;
}

int DfxUnwindTable::FindUnwindTable(struct ElfTableInfo& eti, uintptr_t pc,
    std::shared_ptr<DfxMap> map, std::shared_ptr<DfxElf> elf)
{
    if (IsPcInElfTable(eti, pc) == TABLE_ERROR_NONE) {
        return UNW_ERROR_NONE;
    }

    struct ElfTableInfo edi;
    ResetElfTableInfo(edi);
    if (!GetElfTableInfo(edi, pc, map, elf)) {
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
    LOGE("UnwindTableInfo format: %d", uti.format);
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

    pi.gp = uti.gp;
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
    uintptr_t tableData = uti.tableData;
    LOGU("DwarfSearchUnwindTable segbase:%p, tableData:%p, tableLen: %d",
        (void*)segbase, (void*)tableData, fdeCount);

    // do binary search, encode is stored in symbol file, we have no means to find?
    // hard code for 1b DwarfEncoding
    uintptr_t entry;
    uintptr_t low = 0;
    for (uintptr_t len = fdeCount; len > 1;) {
        uintptr_t cur = low + (len / 2);
        entry = (uintptr_t) tableData + cur * sizeof(DwarfTableEntry);
        LOGU("cur:%d, entry:%llx", (int)cur, (uint64_t)entry);
        int32_t startPcOffset = memory->Read<int32_t>(entry, true);
        uintptr_t startPc = static_cast<uintptr_t>(startPcOffset + segbase);
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
    uintptr_t fdeOffset = static_cast<uintptr_t>(memory->Read<int32_t>(entry, true)) + segbase;
    pi.unwindInfo = (void *) (fdeOffset);
    LOGU("fde offset entry: %llx", (uint64_t)pi.unwindInfo);
    pi.format = UNW_INFO_FORMAT_REMOTE_TABLE;
    pi.gp = uti.gp;
    return UNW_ERROR_NONE;
}
} // namespace HiviewDFX
} // namespace OHOS
