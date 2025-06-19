/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

// workaround for non mingw build
#ifndef EM_LOONGARCH
#define EM_LOONGARCH	258 // LOONGARCH
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
        DFXLOGW("[%{public}d]: ParseElfHeaders failed", __LINE__);
        return false;
    }

    if (!ParseProgramHeaders<EhdrType, PhdrType>(ehdr)) {
        DFXLOGW("[%{public}d]: ParseProgramHeaders failed", __LINE__);
        return false;
    }

    if (!ParseSectionHeaders<EhdrType, ShdrType>(ehdr)) {
        DFXLOGW("ParseSectionHeaders failed");
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
    } else if (machine == EM_RISCV) {
        archType_ = ARCH_RISCV64;
    } else if (machine == EM_X86_64) {
        archType_ = ARCH_X86_64;
    } else if (machine == EM_LOONGARCH) {
        archType_ = ARCH_LOONGARCH;
    } else {
        DFXLOGW("Failed the machine = %{public}d", machine);
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
                if (loadInfo.align == 0) {
                    continue;
                }
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

                if (static_cast<uint64_t>(phdr.p_vaddr) < static_cast<uint64_t>(startVaddr_)) {
                    startVaddr_ = static_cast<uint64_t>(phdr.p_vaddr);
                    startOffset_ = static_cast<uint64_t>(phdr.p_offset);
                }
                if (static_cast<uint64_t>(phdr.p_vaddr + phdr.p_memsz) > static_cast<uint64_t>(endVaddr_)) {
                    endVaddr_ = static_cast<uint64_t>(phdr.p_vaddr + phdr.p_memsz);
                }
                DFXLOGU("Elf startVaddr: %{public}" PRIx64 ", endVaddr: %{public}" PRIx64 "",
                    startVaddr_, endVaddr_);
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

const GnuDebugDataHdr& ElfParser::GetGnuDebugDataHdr() const
{
    return gnuDebugDataHdr_;
}

template <typename EhdrType, typename ShdrType>
bool ElfParser::ExtractSectionHeadersInfo(const EhdrType& ehdr, ShdrType& shdr)
{
    uint64_t offset = ehdr.e_shoff;
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
            DFXLOGE("Failed to get section name");
            continue;
        }

        if (shdr.sh_size != 0 && secName == GNU_DEBUGDATA) {
            gnuDebugDataHdr_.address = reinterpret_cast<uintptr_t>(shdr.sh_offset +
                static_cast<uint8_t *>(mmap_->Get()));
            gnuDebugDataHdr_.size = static_cast<uintptr_t>(shdr.sh_size);
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
            symShdrs_.emplace_back(elfShdr);
        }
    }
    return true;
}

template <typename EhdrType, typename ShdrType>
bool ElfParser::ParseSectionHeaders(const EhdrType& ehdr)
{
    ShdrType shdr;
    //section header string table index. include section header table with section name string table.
    if (ehdr.e_shstrndx < ehdr.e_shnum) {
        uint64_t secOffset = 0;
        uint64_t secSize = 0;
        uint64_t shNdxOffset = ehdr.e_shoff + ehdr.e_shstrndx * ehdr.e_shentsize;
        if (!Read(static_cast<uintptr_t>(shNdxOffset), &shdr, sizeof(shdr))) {
            DFXLOGE("Read section header string table failed");
            return false;
        }
        secOffset = shdr.sh_offset;
        secSize = shdr.sh_size;
        if (!ParseStrTab(sectionNames_, secOffset, secSize)) {
            return false;
        }
    } else {
        DFXLOGE("e_shstrndx(%{public}u) cannot greater than or equal e_shnum(%{public}u)",
            ehdr.e_shstrndx, ehdr.e_shnum);
        return false;
    }

    if (!ExtractSectionHeadersInfo(ehdr, shdr)) {
        return false;
    }

    return true;
}

template <typename DynType>
bool ElfParser::ParseElfDynamic()
{
    if (dynamicOffset_ == 0 || mmap_->Get() == nullptr) {
        return false;
    }

    DynType* dyn = reinterpret_cast<DynType *>(dynamicOffset_ + static_cast<char *>(mmap_->Get()));
    if (dyn == nullptr) {
        return false;
    }
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
    DFXLOGU("parse current elf file soname is %{public}s.", soname_.c_str());
    return true;
}

template <typename SymType>
bool ElfParser::IsFunc(const SymType sym)
{
    return ((sym.st_shndx != SHN_UNDEF) &&
        (ELF32_ST_TYPE(sym.st_info) == STT_FUNC || ELF32_ST_TYPE(sym.st_info) == STT_GNU_IFUNC));
}

template <typename SymType>
bool ElfParser::ParseFuncSymbols()
{
    if (symShdrs_.empty()) {
        return false;
    }

    funcSymbols_.clear();
    for (const auto& iter : symShdrs_) {
        const auto& shdr = iter;
        ParseFuncSymbols<SymType>(shdr);
    }
    return (funcSymbols_.size() > 0);
}

template <typename SymType>
bool ElfParser::ReadSymType(const ElfShdr& shdr, const uint32_t idx, SymType& sym)
{
    uintptr_t offset = static_cast<uintptr_t>(shdr.offset + idx * shdr.entSize);
    if (!Read(offset, &sym, sizeof(sym))) {
        return false;
    }
    if (sym.st_value == 0 || sym.st_size == 0) {
        return false;
    }
    return true;
}

template <typename SymType>
bool ElfParser::ParseFuncSymbols(const ElfShdr& shdr)
{
    ShdrInfo linkShdrInfo;
    if (!GetSectionInfo(linkShdrInfo, shdr.link)) {
        return false;
    }

    uint32_t count = static_cast<uint32_t>((shdr.entSize != 0) ? (shdr.size / shdr.entSize) : 0);
    for (uint32_t idx = 0; idx < count; ++idx) {
        SymType sym;
        if (!ReadSymType(shdr, idx, sym)) {
            continue;
        }
        ElfSymbol elfSymbol;
        if (!ParseFuncSymbolName(linkShdrInfo, sym, elfSymbol.nameStr)) {
            continue;
        }
        elfSymbol.value = static_cast<uint64_t>(sym.st_value);
        elfSymbol.size = static_cast<uint64_t>(sym.st_size);
        elfSymbol.name = static_cast<uint32_t>(sym.st_name);
        funcSymbols_.emplace(elfSymbol);
    }
    DFXLOGU("elfSymbols.size: %{public}" PRIuPTR "", funcSymbols_.size());
    return true;
}

template <typename SymType>
bool ElfParser::ParseFuncSymbolName(const ShdrInfo& linkShdr, SymType sym, std::string& nameStr)
{
    if (!IsFunc(sym) || (static_cast<uint64_t>(sym.st_name) >= linkShdr.size) || mmap_->Get() == nullptr) {
        return false;
    }
    uintptr_t nameOffset = static_cast<uintptr_t>(linkShdr.offset + sym.st_name);
    nameStr = std::string(static_cast<char*>(mmap_->Get()) + nameOffset);
    return true;
}

template <typename SymType>
bool ElfParser::ParseFuncSymbolByAddr(uint64_t addr, ElfSymbol& elfSymbol)
{
    if (symShdrs_.empty()) {
        return false;
    }

    for (const auto& shdr : symShdrs_) {
        ShdrInfo linkShdrInfo;
        if (!GetSectionInfo(linkShdrInfo, shdr.link)) {
            return false;
        }

        uint32_t count = static_cast<uint32_t>((shdr.entSize != 0) ? (shdr.size / shdr.entSize) : 0);
        for (uint32_t idx = 0; idx < count; ++idx) {
            SymType sym;
            if (!ReadSymType(shdr, idx, sym)) {
                continue;
            }

            if ((sym.st_value <= addr) && (addr < (sym.st_value + sym.st_size)) &&
                ParseFuncSymbolName(linkShdrInfo, sym, elfSymbol.nameStr)) {
                elfSymbol.value = static_cast<uint64_t>(sym.st_value);
                elfSymbol.size = static_cast<uint64_t>(sym.st_size);
                elfSymbol.name = static_cast<uint32_t>(sym.st_name);
                DFXLOGU("Parse elf symbol nameStr: %{public}s", elfSymbol.nameStr.c_str());
                return true;
            }
        }
    }
    return false;
}

bool ElfParser::GetSectionNameByIndex(std::string& nameStr, const uint32_t name)
{
    if (sectionNames_.empty() || name >= sectionNames_.size()) {
        DFXLOGE("name index(%{public}u) out of range, size: %{public}" PRIuPTR "", name, sectionNames_.size());
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
        DFXLOGE("size(%{public}" PRIu64 ") is too large.", size);
        return false;
    }
    std::vector<char> namesBuf(size, 0);
    if (!Read((uintptr_t)offset, namesBuf.data(), size)) {
        DFXLOGE("Read failed");
        return false;
    }
    nameStr = std::string(namesBuf.begin(), namesBuf.begin() + size);
    return true;
}

bool ElfParser::GetSectionInfo(ShdrInfo& shdr, const uint32_t idx)
{
    for (const auto& iter: shdrInfoPairs_) {
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
    for (const auto& iter: shdrInfoPairs_) {
        auto tmpPair = iter.first;
        if (tmpPair.second == secName) {
            shdr = iter.second;
            return true;
        }
    }
    return false;
}

bool ElfParser::GetSectionData(unsigned char* buf, uint64_t size, std::string secName)
{
    ShdrInfo shdr;
    if (GetSectionInfo(shdr, secName)) {
        if (Read(shdr.offset, buf, size)) {
            return true;
        }
    } else {
        DFXLOGE("Failed to get data from secName %{public}s", secName.c_str());
    }
    return false;
}

std::string ElfParser::GetBuildId()
{
    ShdrInfo shdr;
    std::string buildId = "";
    if ((GetSectionInfo(shdr, NOTE_GNU_BUILD_ID) || GetSectionInfo(shdr, NOTES)) && mmap_->Get() != nullptr) {
        std::string buildIdHex = ParseHexBuildId((uint64_t)((char *)mmap_->Get() + shdr.offset), shdr.size);
        if (!buildIdHex.empty()) {
            buildId = ToReadableBuildId(buildIdHex);
            DFXLOGU("Elf buildId: %{public}s", buildId.c_str());
        }
    }
    return buildId;
}

std::string ElfParser::ParseHexBuildId(uint64_t noteAddr, uint64_t noteSize)
{
    uint64_t tmp;
    if (__builtin_add_overflow(noteAddr, noteSize, &tmp)) {
        DFXLOGE("noteAddr overflow");
        return "";
    }
    uint64_t offset = 0;
    while (offset < noteSize) {
        ElfW(Nhdr) nhdr;
        if (noteSize - offset < sizeof(nhdr)) {
            return "";
        }
        uint64_t ptr = noteAddr + offset;
        if (memcpy_s(&nhdr, sizeof(nhdr), reinterpret_cast<void*>(ptr), sizeof(nhdr)) != 0) {
            DFXLOGE("memcpy_s nhdr failed");
            return "";
        }
        offset += sizeof(nhdr);
        if (noteSize - offset < nhdr.n_namesz) {
            return "";
        }
        if (nhdr.n_namesz > 0) {
            std::string name(nhdr.n_namesz, '\0');
            ptr = noteAddr + offset;
            if (memcpy_s(&(name[0]), name.size(), reinterpret_cast<void*>(ptr), nhdr.n_namesz) != 0) {
                DFXLOGE("memcpy_s note name failed");
                return "";
            }
            // Trim trailing \0 as GNU is stored as a C string in the ELF file.
            if (name.size() != 0 && name.back() == '\0') {
                name.resize(name.size() - 1);
            }
            // Align nhdr.n_namesz to next power multiple of 4. See man 5 elf.
            offset += (nhdr.n_namesz + 3) & ~3; // 3 : Align the offset to a 4-byte boundary
            if (name != "GNU" || nhdr.n_type != NT_GNU_BUILD_ID) {
                offset += (nhdr.n_descsz + 3) & ~3; // 3 : Align the offset to a 4-byte boundary
                continue;
            }
            if (noteSize - offset < nhdr.n_descsz || nhdr.n_descsz == 0) {
                return "";
            }
            std::string buildIdRaw(nhdr.n_descsz, '\0');
            ptr = noteAddr + offset;
            if (memcpy_s(&buildIdRaw[0], buildIdRaw.size(), reinterpret_cast<void*>(ptr), nhdr.n_descsz) != 0) {
                return "";
            }
            return buildIdRaw;
        }
        // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
        offset += (nhdr.n_descsz + 3) & ~3; // 3 : Align the offset to a 4-byte boundary
    }
    return "";
}

std::string ElfParser::ToReadableBuildId(const std::string& buildIdHex)
{
    if (buildIdHex.empty()) {
        return "";
    }
    const char hexTable[] = "0123456789abcdef";
    const int hexLength = 16;
    const int hexExpandParam = 2;
    const size_t len = buildIdHex.length();
    std::string buildId(len * hexExpandParam, '\0');

    for (size_t i = 0; i < len; i++) {
        unsigned int n = buildIdHex[i];
        buildId[i * hexExpandParam] = hexTable[(n >> 4) % hexLength]; // 4 : higher 4 bit of uint8
        buildId[i * hexExpandParam + 1] = hexTable[n % hexLength];
    }
    return buildId;
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

const std::set<ElfSymbol>& ElfParser32::GetFuncSymbols()
{
    ParseFuncSymbols<Elf32_Sym>();
    return funcSymbols_;
}

const std::set<ElfSymbol>& ElfParser64::GetFuncSymbols()
{
    ParseFuncSymbols<Elf64_Sym>();
    return funcSymbols_;
}

bool ElfParser32::GetFuncSymbolByAddr(uint64_t addr, ElfSymbol& elfSymbol)
{
    return ParseFuncSymbolByAddr<Elf32_Sym>(addr, elfSymbol);
}

bool ElfParser64::GetFuncSymbolByAddr(uint64_t addr, ElfSymbol& elfSymbol)
{
    return ParseFuncSymbolByAddr<Elf64_Sym>(addr, elfSymbol);
}
} // namespace HiviewDFX
} // namespace OHOS
