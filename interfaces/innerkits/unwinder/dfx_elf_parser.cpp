/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "dfx_elf_parser.h"

#include <cstdlib>
#include <elf.h>
#include <link.h>
#include <securec.h>
#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include <iostream>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxElfParser"
}

bool ElfParser::Read(uint64_t pos, void *buf, size_t size)
{
    size_t rc = mmap_->Read(&pos, buf, size);
    if (rc == size) {
        return true;
    }
    return false;
}

size_t ElfParser::MmapSize()
{
    return mmap_->Size();
}

uint64_t ElfParser::GetElfSize()
{
    return elfSize_;
}

template <typename EhdrType, typename PhdrType, typename ShdrType>
bool ElfParser::ParseAllHeaders()
{
    EhdrType ehdr;
    if (!Read(0, &ehdr, sizeof(ehdr))) {
        return false;
    }

    if (!ParseElfHeaders<EhdrType>(ehdr)) {
        DFXLOG_WARN("ParseElfHeaders failed");
        return false;
    }

    if (!ParseProgramHeaders<EhdrType, PhdrType>(ehdr)) {
        DFXLOG_WARN("ParseProgramHeaders failed");
        return false;
    }

    if (!ParseSectionHeaders<EhdrType, ShdrType>(ehdr)) {
        DFXLOG_WARN("ReadSectionHeaders failed");
        return false;
    }
    return true;
}

template <typename EhdrType>
bool ElfParser::ParseElfHeaders(const EhdrType& ehdr)
{
    if (ehdr.e_shnum == 0) {
        return false;
    }

    auto machine = ehdr.e_machine;
    if (machine == EM_ARM) {
        archType_ = ARCH_ARM;
    } else if (machine == EM_386) {
        archType_ = ARCH_X86;
    } else if (machine == EM_AARCH64) {
        archType_ = ARCH_ARM64;
    } else if (machine == EM_X86_64) {
        archType_ = ARCH_X86_64;
    } else {
        LOGW("Failed the machine = %d", machine);
    }
    elfSize_ = ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum;
    return true;
}

template <typename EhdrType, typename PhdrType>
bool ElfParser::ParseProgramHeaders(const EhdrType& ehdr)
{
    uint64_t offset = ehdr.e_phoff;
    bool firstLoadHeader = true;
    for (size_t i = 0; i < ehdr.e_phnum; i++, offset += ehdr.e_phentsize) {
        PhdrType phdr;
        if (!Read(offset, &phdr, sizeof(phdr))) {
            return false;
        }

        switch (phdr.p_type) {
        case PT_LOAD: {
            if ((phdr.p_flags & PF_X) == 0) {
                continue;
            }
            //LOGU("phdr.p_offset: %llx, phdr.p_vaddr: %llx", phdr.p_offset, phdr.p_vaddr);
            ptLoads_[phdr.p_offset] = ElfLoadInfo{phdr.p_offset, phdr.p_vaddr, static_cast<size_t>(phdr.p_memsz)};

            // Only set the load bias from the first executable load header.
            if (firstLoadHeader) {
                loadBias_ = static_cast<int64_t>(static_cast<uint64_t>(phdr.p_vaddr) - phdr.p_offset);
            }
            firstLoadHeader = false;

            if (phdr.p_vaddr < startVaddr_) {
                startVaddr_ = phdr.p_vaddr;
            }
            if (phdr.p_vaddr + phdr.p_memsz > endVaddr_) {
                endVaddr_ = phdr.p_vaddr + phdr.p_memsz;
            }
            break;
        }
        case PT_DYNAMIC: {
            pDynamic_ = reinterpret_cast<void *>(&phdr);
            break;
        }
        case PT_ARM_EXIDX: {
            pArmExidx_ = reinterpret_cast<void *>(&phdr);
            break;
        }
        case PT_GNU_EH_FRAME: {
            pEhHdr_ = reinterpret_cast<void *>(&phdr);
            break;
        }
        default:
            break;
        }
    }
    return true;
}

template <typename EhdrType, typename ShdrType>
bool ElfParser::ParseSectionHeaders(const EhdrType& ehdr)
{
    uint64_t offset = ehdr.e_shoff;
    uint64_t secOffset = 0;
    uint64_t secSize = 0;

    ShdrType shdr;
    //section header string table index. include section header table with section name string table.
    if (ehdr.e_shstrndx < ehdr.e_shnum) {
        uint64_t shNdxOffset = offset + ehdr.e_shstrndx * ehdr.e_shentsize;
        if (!Read(shNdxOffset, &shdr, sizeof(shdr))) {
            LOGE("Read section header string table failed");
            return false;
        }
        secOffset = shdr.sh_offset;
        secSize = shdr.sh_size;
        if (!ParseStrTab(sectionNames_, secOffset, secSize)) {
            return false;
        }
    } else {
        LOGE("e_shstrndx(%u) cannot greater than or equal e_shnum(%u)", ehdr.e_shstrndx, ehdr.e_shnum);
		return false;
    }

    // Skip the first header, it's always going to be NULL.
    offset += ehdr.e_shentsize;
    for (size_t i = 1; i < ehdr.e_shnum; i++, offset += ehdr.e_shentsize) {
        if (i == ehdr.e_shstrndx) {
            continue;
        }
        if (!Read(offset, &shdr, sizeof(shdr))) {
            return false;
        }

        std::string secName;
        if (!GetSectionNameByIndex(secName, shdr.sh_name)) {
            LOGE("Failed to get section name");
            continue;
        }
        //LOGU("ParseSectionHeaders secName: %s", secName.c_str());

        ShdrInfo shdrInfo;
        shdrInfo.addr = static_cast<uint64_t>(shdr.sh_addr);
        shdrInfo.size = static_cast<uint64_t>(shdr.sh_size);
        shdrInfo.offset = static_cast<uint64_t>(shdr.sh_offset);
        shdrInfos_.emplace(secName, shdrInfo);

        elfSecInfos_[i] = ElfSecInfo{secName, shdrInfo};

        if (shdr.sh_type == SHT_SYMTAB ||
            shdr.sh_type == SHT_DYNSYM) {
            ElfShdr elfShdr;
            elfShdr.name = static_cast<uint32_t>(shdr.sh_name);
            elfShdr.type = static_cast<uint32_t>(shdr.sh_type);
            elfShdr.flags = static_cast<uint64_t>(shdr.sh_flags);
            elfShdr.addr = static_cast<uint64_t>(shdr.sh_addr);
            elfShdr.offset = static_cast<uint64_t>(shdr.sh_offset);
            elfShdr.size = static_cast<uint64_t>(shdr.sh_size);
            elfShdr.link = static_cast<uint32_t>(shdr.sh_link);
            elfShdr.info = static_cast<uint32_t>(shdr.sh_info);
            elfShdr.addrAlign = static_cast<uint64_t>(shdr.sh_addralign);
            elfShdr.entSize = static_cast<uint64_t>(shdr.sh_entsize);
            symShdrs_.emplace(secName, elfShdr);
        }
    }
    return true;
}

template <typename NhdrType>
std::string ElfParser::ParseBuildId(uint64_t noteOffset, uint64_t noteSize)
{
    uint64_t tmp;
    if (__builtin_add_overflow(noteOffset, noteSize, &tmp)) {
        return "";
    }

    uint64_t offset = 0;
    while (offset < noteSize) {
        if (noteSize - offset < sizeof(NhdrType)) {
            return "";
        }
        NhdrType hdr;
        if (!Read(noteOffset + offset, &hdr, sizeof(hdr))) {
            return "";
        }
        offset += sizeof(hdr);
        if (noteSize - offset < hdr.n_namesz) {
            return "";
        }
        if (hdr.n_namesz > 0) {
            std::string name(hdr.n_namesz, '\0');
            if (!Read(noteOffset + offset, &(name[0]), hdr.n_namesz)) {
                return "";
            }
            // Trim trailing \0 as GNU is stored as a C string in the ELF file.
            if (name.back() == '\0') {
                name.resize(name.size() - 1);
            }

            // Align hdr.n_namesz to next power multiple of 4. See man 5 elf.
            offset += (hdr.n_namesz + 3) & ~3;
            if (name == "GNU" && hdr.n_type == NT_GNU_BUILD_ID) {
                if (noteSize - offset < hdr.n_descsz || hdr.n_descsz == 0) {
                    return "";
                }
                std::string buildIdRaw(hdr.n_descsz, '\0');
                if (Read(noteOffset + offset, &buildIdRaw[0], hdr.n_descsz)) {
                    return buildIdRaw;
                }
            }
        }
        // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
        offset += (hdr.n_descsz + 3) & ~3;
    }
    return "";
}

template <typename DynType>
std::string ElfParser::ParseElfName()
{
    return "";
}

template <typename SymType>
bool ElfParser::ParseElfSymbols()
{
    if (symShdrs_.empty()) {
        return false;
    }

    for (const auto &iter : symShdrs_) {
        const auto &shdr = iter.second;
        if (shdr.type != SHT_SYMTAB && shdr.type != SHT_DYNSYM) {
            LOGE("shdr.type: %d", shdr.type);
            continue;
        }

        SymType sym;
        //LOGU("shdr.offset: %llx, size: %llx, entSize: %llx, link: %d",
        //    shdr.offset, shdr.size, shdr.entSize, shdr.link);
        uint64_t offset = shdr.offset;
        const char* strtabPtr = GetStrTabPtr(shdr.link);
        for (; offset < shdr.size; offset += shdr.entSize) {
            if (!Read(offset, &sym, sizeof(sym))) {
                continue;
            }

            ElfSymbol elfSymbol;
            elfSymbol.name = static_cast<uint32_t>(sym.st_name);
            if (strtabPtr != nullptr) {
                elfSymbol.nameStr = std::string(strtabPtr + elfSymbol.name);
                //LOGU("elfSymbol.nameStr: %s", elfSymbol.nameStr.c_str());
            }
            elfSymbol.info = sym.st_info;
            elfSymbol.other = sym.st_other;
            elfSymbol.shndx = static_cast<uint16_t>(sym.st_shndx);
            elfSymbol.value = static_cast<uint64_t>(sym.st_value);
            elfSymbol.size = static_cast<uint64_t>(sym.st_size);
            elfSymbols_.push_back(elfSymbol);
        }
    }
    LOGU("elfSymbols.size: %d", elfSymbols_.size());
    return (elfSymbols_.size() > 0);
}

bool ElfParser::GetSectionNameByIndex(std::string& nameStr, const uint32_t name)
{
    if (sectionNames_.empty() || name >= sectionNames_.size()) {
        LOGE("name index(%u) out of range, size: %d", name, sectionNames_.size());
        return false;
    }

    size_t endIndex = sectionNames_.find('\0', name);
    if (endIndex != std::string::npos) {
        nameStr = sectionNames_.substr(name, endIndex - name);
        return true;
    }
    return false;
}

const char* ElfParser::GetStrTabPtr(const uint32_t link)
{
    if (elfSecInfos_.find(link) == elfSecInfos_.end()) {
        return nullptr;
    }
    auto secInfo = elfSecInfos_[link];
    return ((char *)mmap_->Get() + secInfo.shdrInfo.offset);
}

bool ElfParser::ParseStrTab(std::string& nameStr, const uint64_t offset, const uint64_t size)
{
    if (size > MmapSize()) {
        LOGE("size(%" PRIu64 ") is too large.", size);
        return false;
    }
    char *namesBuf = new char[size];
    if (namesBuf == nullptr) {
        LOGE("New failed");
        return false;
    }
    (void)memset_s(namesBuf, size, '\0', size);
    if (!Read(offset, namesBuf, size)) {
        LOGE("Read failed");
        delete[] namesBuf;
        namesBuf = nullptr;
        return false;
    }
    nameStr = std::string(namesBuf, namesBuf + size);
    delete[] namesBuf;
    namesBuf = nullptr;
    return true;
}

bool ElfParser::GetSectionInfo(ShdrInfo& shdr, const std::string secName)
{
    if (shdrInfos_.find(secName) != shdrInfos_.end()) {
        shdr = shdrInfos_[secName];
        return true;
    }
    return false;
}

bool ElfParser::GetSymSection(ElfShdr& shdr, const std::string secName)
{
    if (symShdrs_.find(secName) != symShdrs_.end()) {
        shdr = symShdrs_[secName];
        return true;
    }
    return false;
}

bool ElfParser32::InitHeaders()
{
    return ParseAllHeaders<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>();
}

bool ElfParser64::InitHeaders()
{
    return ParseAllHeaders<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>();
}

std::string ElfParser32::GetBuildId()
{
    ShdrInfo shdr;
    if (GetSectionInfo(shdr, NOTE_GNU_BUILD_ID)){
        return ParseBuildId<Elf32_Nhdr>(shdr.offset, shdr.size);
    }
    return "";
}

std::string ElfParser64::GetBuildId()
{
    ShdrInfo shdr;
    if (GetSectionInfo(shdr, NOTE_GNU_BUILD_ID)){
        return ParseBuildId<Elf64_Nhdr>(shdr.offset, shdr.size);
    }
    return "";
}

std::string ElfParser32::GetElfName()
{
    return ParseElfName<Elf32_Dyn>();
}

std::string ElfParser64::GetElfName()
{
    return ParseElfName<Elf64_Dyn>();
}

const std::vector<ElfSymbol>& ElfParser32::GetElfSymbols()
{
    if (elfSymbols_.empty()) {
        ParseElfSymbols<Elf32_Sym>();
    }
    return elfSymbols_;
}

const std::vector<ElfSymbol>& ElfParser64::GetElfSymbols()
{
    if (elfSymbols_.empty()) {
        ParseElfSymbols<Elf64_Sym>();
    }
    return elfSymbols_;
}
} // namespace HiviewDFX
} // namespace OHOS