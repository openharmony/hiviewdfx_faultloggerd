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

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <securec.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"

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

bool ElfParser::Read(uintptr_t pos, void *buf, size_t size)
{
    if (mmap_->Read(pos, buf, size) == size) {
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
        DFXLOG_WARN("ParseSectionHeaders failed");
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
        if (!Read((uintptr_t)offset, &phdr, sizeof(phdr))) {
            return false;
        }

        switch (phdr.p_type) {
            case PT_LOAD: {
                ElfLoadInfo loadInfo;
                loadInfo.offset = phdr.p_offset;
                loadInfo.tableVaddr = phdr.p_vaddr;
                loadInfo.tableSize = static_cast<size_t>(phdr.p_memsz);
                loadInfo.align = phdr.p_align;
                uint64_t len = loadInfo.tableSize + (loadInfo.tableVaddr & (loadInfo.align - 1));
                loadInfo.mmapLen = len - (len & (loadInfo.align - 1)) + loadInfo.align;
                ptLoads_[phdr.p_offset] = loadInfo;
                if ((phdr.p_flags & PF_X) == 0) {
                    continue;
                }
                // Only set the load bias from the first executable load header.
                if (firstLoadHeader) {
                    loadBias_ = static_cast<int64_t>(static_cast<uint64_t>(phdr.p_vaddr) - phdr.p_offset);
                }
                firstLoadHeader = false;

                if (phdr.p_vaddr < startVaddr_) {
                    startVaddr_ = phdr.p_vaddr;
                    startOffset_ = phdr.p_offset;
                }
                if (phdr.p_vaddr + phdr.p_memsz > endVaddr_) {
                    endVaddr_ = phdr.p_vaddr + phdr.p_memsz;
                }
                LOGU("Elf startVaddr: %llx, endVaddr: %llx", (uint64_t)startVaddr_, (uint64_t)endVaddr_);
                break;
            }
            case PT_DYNAMIC: {
                dynamicOffset_ = phdr.p_offset;
                break;
            }
            default:
                break;
        }
    }
    return true;
}

void ElfParser::EnableMiniDebugInfo()
{
    enableMiniDebugInfo_ = true;
}

std::shared_ptr<MiniDebugInfo> ElfParser::GetMiniDebugInfo()
{
    return minidebugInfo_;
}

template <typename EhdrType, typename ShdrType>
bool ElfParser::ParseSectionHeaders(const EhdrType& ehdr)
{
    uint64_t offset = ehdr.e_shoff;

    ShdrType shdr;
    //section header string table index. include section header table with section name string table.
    if (ehdr.e_shstrndx < ehdr.e_shnum) {
        uint64_t secOffset = 0;
        uint64_t secSize = 0;
        uint64_t shNdxOffset = offset + ehdr.e_shstrndx * ehdr.e_shentsize;
        if (!Read((uintptr_t)shNdxOffset, &shdr, sizeof(shdr))) {
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

    offset += ehdr.e_shentsize;
    for (size_t i = 1; i < ehdr.e_shnum; i++, offset += ehdr.e_shentsize) {
        if (i == ehdr.e_shstrndx) {
            continue;
        }
        if (!Read((uintptr_t)offset, &shdr, sizeof(shdr))) {
            return false;
        }

        std::string secName;
        if (!GetSectionNameByIndex(secName, shdr.sh_name)) {
            LOGE("Failed to get section name");
            continue;
        }

        if (enableMiniDebugInfo_) {
            if (shdr.sh_size != 0 && secName == GNU_DEBUGDATA) {
                minidebugInfo_ = std::make_shared<MiniDebugInfo>();
                minidebugInfo_->offset = shdr.sh_offset;
                minidebugInfo_->size = shdr.sh_size;
            }
        }

        ShdrInfo shdrInfo;
        shdrInfo.addr = static_cast<uint64_t>(shdr.sh_addr);
        shdrInfo.entSize = static_cast<uint64_t>(shdr.sh_entsize);
        shdrInfo.size = static_cast<uint64_t>(shdr.sh_size);
        shdrInfo.offset = static_cast<uint64_t>(shdr.sh_offset);
        shdrInfoPairs_.emplace(std::make_pair(i, secName), shdrInfo);

        if (shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_DYNSYM) {
            if (shdr.sh_link >= ehdr.e_shnum) {
                continue;
            }
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
            symShdrs_.push_back(elfShdr);
        }
    }
    return true;
}

template <typename DynType>
bool ElfParser::ParseElfDynamic()
{
    if (dynamicOffset_ == 0) {
        return false;
    }

    DynType *dyn = (DynType *)(dynamicOffset_ + static_cast<char*>(mmap_->Get()));
    for (; dyn->d_tag != DT_NULL; ++dyn) {
        if (dyn->d_tag == DT_PLTGOT) {
            // Assume that _DYNAMIC is writable and GLIBC has relocated it (true for x86 at least).
            dtPltGotAddr_ = dyn->d_un.d_ptr;
            break;
        } else if (dyn->d_tag == DT_STRTAB) {
            dtStrtabAddr_ = dyn->d_un.d_ptr;
        } else if (dyn->d_tag == DT_STRSZ) {
            dtStrtabSize_ = dyn->d_un.d_val;
        } else if (dyn->d_tag == DT_SONAME) {
            dtSonameOffset_ = dyn->d_un.d_val;
        }
    }
    return true;
}

template <typename DynType>
bool ElfParser::ParseElfName()
{
    if (!ParseElfDynamic<DynType>()) {
        return false;
    }
    ShdrInfo shdrInfo;
    if (!GetSectionInfo(shdrInfo, DYNSTR)) {
        return false;
    }
    uintptr_t sonameOffset = shdrInfo.offset + dtSonameOffset_;
    uint64_t sonameOffsetMax = shdrInfo.offset + dtStrtabSize_;
    size_t maxStrSize = static_cast<size_t>(sonameOffsetMax - sonameOffset);
    mmap_->ReadString(sonameOffset, &soname_, maxStrSize);
    LOGU("parse current elf file soname is %s.", soname_.c_str());
    return true;
}

template <typename SymType>
bool ElfParser::IsFunc(const SymType sym)
{
    return ((sym.st_shndx != SHN_UNDEF) &&
        (ELF32_ST_TYPE(sym.st_info) == STT_FUNC || ELF32_ST_TYPE(sym.st_info) == STT_GNU_IFUNC));
}

template <typename SymType>
bool ElfParser::ParseElfSymbols(bool isFunc, bool isSort)
{
    if (symShdrs_.empty()) {
        return false;
    }

    elfSymbols_.clear();
    for (const auto &iter : symShdrs_) {
        const auto &shdr = iter;
        ParseElfSymbols<SymType>(shdr, isFunc, isSort);
    }
    return (elfSymbols_.size() > 0);
}

template <typename SymType>
bool ElfParser::ParseElfSymbols(ElfShdr shdr, bool isFunc, bool isSort)
{
    if (shdr.type != SHT_SYMTAB && shdr.type != SHT_DYNSYM) {
        LOGE("shdr.type: %d", shdr.type);
        return false;
    }

    ShdrInfo shdrInfo;
    if (!GetSectionInfo(shdrInfo, shdr.link)) {
        return false;
    }

    uint32_t count = static_cast<uint32_t>((shdr.entSize != 0) ? (shdr.size / shdr.entSize) : 0);
    LOGU("count: %d", count);
    for (uint32_t idx = 0; idx < count; ++idx) {
        ElfSymbol elfSymbol;
        if (!ParseElfSymbol<SymType>(shdr, idx, isFunc, elfSymbol)) {
            continue;
        }
        elfSymbol.strOffset = shdrInfo.offset;
        elfSymbol.strSize = shdrInfo.size;
        elfSymbols_.push_back(elfSymbol);
    }

    if (isSort) {
        auto comp = [](ElfSymbol a, ElfSymbol b) { return a.value < b.value; };
        std::sort(elfSymbols_.begin(), elfSymbols_.end(), comp);
    }
    auto pred = [](ElfSymbol a, ElfSymbol b) { return a.value == b.value; };
    elfSymbols_.erase(std::unique(elfSymbols_.begin(), elfSymbols_.end(), pred), elfSymbols_.end());
    elfSymbols_.shrink_to_fit();
    LOGU("elfSymbols.size: %d", elfSymbols_.size());
    return true;
}

template <typename SymType>
bool ElfParser::ParseElfSymbol(ElfShdr shdr, uint32_t idx, bool isFunc, ElfSymbol& elfSymbol)
{
    uintptr_t offset = static_cast<uintptr_t>(shdr.offset + idx * shdr.entSize);
    SymType sym;
    if (!Read(offset, &sym, sizeof(sym))) {
        return false;
    }

    if (isFunc && !IsFunc(sym)) {
        return false;
    }
    elfSymbol.value = static_cast<uint64_t>(sym.st_value);
    elfSymbol.size = static_cast<uint64_t>(sym.st_size);
    elfSymbol.name = static_cast<uint32_t>(sym.st_name);
    return true;
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
    if (!Read((uintptr_t)offset, namesBuf, size)) {
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

bool ElfParser::GetSectionInfo(ShdrInfo& shdr, const uint32_t idx)
{
    for (const auto &iter: shdrInfoPairs_) {
        auto tmpPair = iter.first;
        if (tmpPair.first == idx) {
            shdr = iter.second;
            return true;
        }
    }
    return false;
}

bool ElfParser::GetSectionInfo(ShdrInfo& shdr, const std::string& secName)
{
    for (const auto &iter: shdrInfoPairs_) {
        auto tmpPair = iter.first;
        if (tmpPair.second == secName) {
            shdr = iter.second;
            return true;
        }
    }
    return false;
}
bool ElfParser::GetSectionData(unsigned char *buf, uint64_t size, std::string secName)
{
    ShdrInfo shdr;
    if (GetSectionInfo(shdr, secName)) {
        if (Read(shdr.offset, buf, size)) {
            return true;
        }
    } else {
        LOGE("string not found secName %u ", secName.c_str());
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

std::string ElfParser32::GetElfName()
{
    if (soname_ == "") {
        ParseElfName<Elf32_Dyn>();
    }
    return soname_;
}

std::string ElfParser64::GetElfName()
{
    if (soname_ == "") {
        ParseElfName<Elf64_Dyn>();
    }
    return soname_;
}

uintptr_t ElfParser32::GetGlobalPointer()
{
    if (dtPltGotAddr_ == 0) {
        ParseElfDynamic<Elf32_Dyn>();
    }
    return dtPltGotAddr_;
}

uintptr_t ElfParser64::GetGlobalPointer()
{
    if (dtPltGotAddr_ == 0) {
        ParseElfDynamic<Elf64_Dyn>();
    }
    return dtPltGotAddr_;
}

const std::vector<ElfSymbol>& ElfParser32::GetElfSymbols(bool isFunc, bool isSort)
{
    ParseElfSymbols<Elf32_Sym>(isFunc, isSort);
    return elfSymbols_;
}

const std::vector<ElfSymbol>& ElfParser64::GetElfSymbols(bool isFunc, bool isSort)
{
    ParseElfSymbols<Elf64_Sym>(isFunc, isSort);
    return elfSymbols_;
}
} // namespace HiviewDFX
} // namespace OHOS