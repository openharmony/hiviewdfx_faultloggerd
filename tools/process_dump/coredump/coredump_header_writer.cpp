/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "coredump_header_writer.h"

#include "coredump_mapping_manager.h"
#include "dfx_log.h"
#include "securec.h"
#include "parameters.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
    constexpr int ALIGN_SIZE = 8;
}

bool ProgramSegmentHeaderWriter::Write()
{
    Elf64_Phdr ptNote;
    PtNoteHeaderFill(ptNote);
    if (!bw_.Write(&ptNote, sizeof(ptNote))) {
        DFXLOGE("Write ptNote fail, errno:%{public}d", errno);
        return false;
    }

    Elf64_Phdr ptLoad;
    Elf64_Half lineNumber = 1;

    for (const auto &region : maps_) {
        if (!CoredumpMappingManager::ShouldIncludeRegion(region)) {
            continue;
        }
        PtLoadHeaderFill(ptLoad, region);
        if (!bw_.Write(&ptLoad, sizeof(ptLoad))) {
            DFXLOGE("Write ptLoad fail, errno:%{public}d", errno);
            return false;
        }
        lineNumber++;
    }

    char *ptNoteAddr = bw_.GetBase() + sizeof(Elf64_Ehdr);
    char *ptLoadAddr = ptNoteAddr + sizeof(Elf64_Phdr);

    char *pOffsetAddr = ptLoadAddr + offsetof(Elf64_Phdr, p_offset);
    Elf64_Off *pOffset = reinterpret_cast<Elf64_Off *>(pOffsetAddr);
    *pOffset = sizeof(Elf64_Ehdr) + (lineNumber * sizeof(Elf64_Phdr));

    char *pFileszAddrLast = ptLoadAddr + offsetof(Elf64_Phdr, p_filesz);
    Elf64_Xword *pFileszLast = reinterpret_cast<Elf64_Xword *>(pFileszAddrLast);

    Elf64_Off pOffsetValueLast = *pOffset;
    for (Elf64_Half i = 0; i < (lineNumber - 2); i++) { // 2 : jump PT_NOTE and first PT_LOAD
        pOffsetAddr += sizeof(Elf64_Phdr);
        pOffset = reinterpret_cast<Elf64_Off *>(pOffsetAddr);

        *pOffset = pOffsetValueLast + *pFileszLast;

        pOffsetValueLast = *pOffset;
        pFileszAddrLast += sizeof(Elf64_Phdr);
        pFileszLast = reinterpret_cast<Elf64_Xword *>(pFileszAddrLast);
    }

    pOffsetAddr = ptNoteAddr + offsetof(Elf64_Phdr, p_offset);
    pOffset = reinterpret_cast<Elf64_Off *>(pOffsetAddr);
    *pOffset = pOffsetValueLast + *pFileszLast;

    ePhnum_ = lineNumber;
    return true;
}

void ProgramSegmentHeaderWriter::PtLoadHeaderFill(Elf64_Phdr &ph, const DumpMemoryRegions &region)
{
    Elf64_Word pFlags = 0x00;
    if (region.priority[0] == 'r' || region.priority[0] == '-') { // 0 : readable
        pFlags |= PF_R;
    }
    if (region.priority[1] == 'w') { // 1 : writable
        pFlags |= PF_W;
    }
    if (region.priority[2] == 'x') { // 2 : executable
        pFlags |= PF_X;
    }
    ProgramSegmentHeaderFill(ph, PT_LOAD, pFlags, region);
}

void ProgramSegmentHeaderWriter::PtNoteHeaderFill(Elf64_Phdr &ph)
{
    ProgramSegmentHeaderFill(ph, PT_NOTE, PF_R, {});
}

void ProgramSegmentHeaderWriter::ProgramSegmentHeaderFill(Elf64_Phdr &ph, Elf64_Word pType, Elf64_Word pFlags,
    const DumpMemoryRegions &region)
{
    ph.p_type = pType;
    ph.p_flags = pFlags;
    ph.p_offset = sizeof(Elf64_Ehdr);
    ph.p_vaddr = region.startHex;
    ph.p_paddr = 0x00;
    ph.p_filesz = region.memorySizeHex;
    ph.p_memsz = ph.p_filesz;
    ph.p_align = 0x1;
}

void SectionHeaderTableWriter::SetShFlag(const Elf64_Phdr* programHeader, Elf64_Xword &shFlag)
{
    if (programHeader->p_flags == PF_R) {
        shFlag = SHF_ALLOC;
    } else if (programHeader->p_flags == (PF_R | PF_W)) {
        shFlag = SHF_WRITE | SHF_ALLOC;
    } else if (programHeader->p_flags == (PF_R | PF_X)) {
        shFlag = SHF_ALLOC | SHF_EXECINSTR;
    }
}

bool SectionHeaderTableWriter::WriteSystemVersion(Elf64_Shdr *sectionHeader, char *versionStartAddr, size_t verLen)
{
    if (sectionHeader == nullptr || versionStartAddr == nullptr) {
        return false;
    }
    sectionHeader->sh_name = 0x17; // 0x17 : .note.dfx.system.version offset
    sectionHeader->sh_addr = 0x00;
    sectionHeader->sh_type = SHT_NOTE;
    sectionHeader->sh_flags = 0x00;
    sectionHeader->sh_offset = static_cast<Elf64_Off>(versionStartAddr - bw_.GetBase());
    sectionHeader->sh_size = verLen + 1;
    return true;
}

bool SectionHeaderTableWriter::Write()
{
    char *versionStartAddr = bw_.GetCurrent();
    std::string buildInfo = "Build info:" + OHOS::system::GetParameter("const.product.software.version", "Unknown");
    bw_.Write(buildInfo.c_str(), buildInfo.length() + 1);

    Elf64_Ehdr *elfHeader = reinterpret_cast<Elf64_Ehdr *>(bw_.GetBase());
    Elf64_Phdr *programHeader = reinterpret_cast<Elf64_Phdr *>(bw_.GetBase() + sizeof(Elf64_Ehdr));

    char *strTableAddr = bw_.GetCurrent();
    char strTable[] = "\0.note\0.shstrtab\0.load\0.note.dfx.system.version\0";
    if (!bw_.Write(strTable, sizeof(strTable))) {
        DFXLOGE("Wrtie strTable fail, errno:%{public}d", errno);
        return false;
    }

    Elf64_Off offset = Elf64_Off(bw_.GetCurrent() - bw_.GetBase());
    uint8_t remain = offset % ALIGN_SIZE;
    AdjustOffset(remain);
    elfHeader->e_shoff = static_cast<Elf64_Off>(bw_.GetCurrent() - bw_.GetBase());
    (void)memset_s(bw_.GetCurrent(), sizeof(Elf64_Shdr), 0, sizeof(Elf64_Shdr));
    bw_.Advance(sizeof(Elf64_Shdr));

    Elf64_Shdr *sectionHeader = reinterpret_cast<Elf64_Shdr *>(bw_.GetCurrent());
    sectionHeader->sh_name = 0x01; // 0x01 : .note offset
    SectionHeaderFill(sectionHeader, SHT_NOTE, 0x00, programHeader);
    sectionHeader += 1;
    programHeader += 1;
    for (int i = 0; i < elfHeader->e_shnum - 4; i++) { // 4 : jump special section header
        sectionHeader->sh_name = 0x11; // 0x11 : .load offset
        Elf64_Xword shFlag = 0x00;
        SetShFlag(programHeader, shFlag);
        SectionHeaderFill(sectionHeader, SHT_PROGBITS, shFlag, programHeader);
        sectionHeader += 1;
        programHeader += 1;
    }
    sectionHeader->sh_name = 0x07; // 0x07 : .shstrtab offset
    SectionHeaderFill(sectionHeader, SHT_STRTAB, SHF_ALLOC, programHeader);
    sectionHeader->sh_addr = 0x00;
    sectionHeader->sh_offset = static_cast<Elf64_Off>(strTableAddr - bw_.GetBase());
    sectionHeader->sh_size = sizeof(strTable);
    sectionHeader += 1;
    WriteSystemVersion(sectionHeader, versionStartAddr, buildInfo.length());
    sectionHeader += 1;
    bw_.SetCurrent(reinterpret_cast<char*>(sectionHeader));
    return true;
}

void SectionHeaderTableWriter::AdjustOffset(uint8_t remain)
{
    if (remain > ALIGN_SIZE) {
        return;
    }
    for (uint8_t i = 0; i < (ALIGN_SIZE - remain); i++) {
        if (remain == 0) {
            break;
        }
        *bw_.GetCurrent() = 0;
        bw_.Advance(1);
    }
}

void SectionHeaderTableWriter::SectionHeaderFill(Elf64_Shdr *sectionHeader, Elf64_Word shType,
    Elf64_Xword shFlag, Elf64_Phdr *programHeader)
{
    sectionHeader->sh_type = shType;
    sectionHeader->sh_flags = shFlag;
    sectionHeader->sh_addr = programHeader->p_vaddr;
    sectionHeader->sh_offset = programHeader->p_offset;
    sectionHeader->sh_size = programHeader->p_filesz;
    sectionHeader->sh_link = 0x00;
    sectionHeader->sh_info = 0x00;
    sectionHeader->sh_addralign = 0x1;
    sectionHeader->sh_entsize = 0x00;
}

bool CoredumpElfHeaderWriter::Write()
{
    if (!bufferWriter_.WriteAt(&ehdr_, sizeof(ehdr_), 0)) {
        DFXLOGE("write elf header fail");
        return false;
    }
    return true;
}

void CoredumpElfHeaderWriter::InitElfHeader(uint16_t ePhnum)
{
    ehdr_.e_ident[EI_MAG0] = ELFMAG0;
    ehdr_.e_ident[EI_MAG1] = ELFMAG1;
    ehdr_.e_ident[EI_MAG2] = ELFMAG2;
    ehdr_.e_ident[EI_MAG3] = ELFMAG3;
    ehdr_.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr_.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr_.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr_.e_ident[EI_OSABI] = ELFOSABI_NONE;
    ehdr_.e_ident[EI_ABIVERSION] = 0x00;
    ehdr_.e_ident[EI_PAD] = 0x00;
    ehdr_.e_type = ET_CORE;
    ehdr_.e_machine = EM_AARCH64;
    ehdr_.e_version = EV_CURRENT;
    ehdr_.e_entry = 0x00;
    ehdr_.e_phoff = sizeof(Elf64_Ehdr);
    ehdr_.e_shoff = sizeof(Elf64_Shdr);
    ehdr_.e_flags = 0x00;
    ehdr_.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr_.e_phentsize = sizeof(Elf64_Phdr);
    ehdr_.e_phnum = ePhnum;
    ehdr_.e_shentsize = sizeof(Elf64_Shdr);
    ehdr_.e_shnum = ePhnum + 3; // 3: jump .note ... sec
    ehdr_.e_shstrndx = ePhnum + 1;
}
} // namespace HiviewDFX
} // namespace OHOS
