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

#include "dfx_elf_parse.h"

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
#define LOG_TAG "DfxElfParse"
}

bool ElfParse::Read(uint64_t pos, void *buf, size_t size)
{
    size_t rc = mmap_->Read(&pos, buf, size);
    if (rc == size) {
        return true;
    }
    return false;
}

size_t ElfParse::Size()
{
    return mmap_->Size();
}

std::string ElfParse::GetElfName()
{
    return "";
}

int64_t ElfParse::GetLoadBias()
{
    return 0;
}

template <typename EhdrType, typename PhdrType, typename ShdrType>
bool ElfParse::ParseAllHeaders()
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
bool ElfParse::ParseElfHeaders(const EhdrType& ehdr)
{
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
        return false;
    }
    return true;
}

template <typename EhdrType, typename PhdrType>
bool ElfParse::ParseProgramHeaders(const EhdrType& ehdr)
{
    uint64_t offset = ehdr.e_phoff;
    bool firstLoadHeader = true;
    for (size_t i = 0; i < ehdr.e_phnum; i++, offset += ehdr.e_phentsize) {
        PhdrType phdr;
        if (!Read(offset, &phdr, sizeof(phdr))) {
            return false;
        }
        switch (phdr.p_type) {
            case PT_LOAD:
                if ((phdr.p_flags & PF_X) == 0) {
                    break;
                }
                ptLoads_[phdr.p_offset] = ElfLoadInfo{phdr.p_offset, phdr.p_vaddr, static_cast<size_t>(phdr.p_memsz)};
                // Only set the load bias from the first executable load header.
                if (firstLoadHeader) {
                    loadBias_ = static_cast<int64_t>(static_cast<uint64_t>(phdr.p_vaddr) - phdr.p_offset);
                }
                firstLoadHeader = false;
                break;
            default:
                LOGW("Failed parse phdr.p_type = %u", phdr.p_type);
                break;
        }
    }
    return true;
}

template <typename EhdrType, typename ShdrType>
bool ElfParse::ParseSectionHeaders(const EhdrType& ehdr)
{
    uint64_t offset = ehdr.e_shoff;
    uint64_t secOffset = 0;
    uint64_t secSize = 0;

    ShdrType shdr;
    //section header string table index. 包含了section header table 中 section name string table。
    if (ehdr.e_shstrndx < ehdr.e_shnum) {
        uint64_t shNdxOffset = offset + ehdr.e_shstrndx * ehdr.e_shentsize;
        if (!Read(shNdxOffset, &shdr, sizeof(shdr))) {
            LOGE("Read section header string table failed");
            return false;
        }
        secOffset = shdr.sh_offset;
        secSize = shdr.sh_size;
        if (secSize > Size()) {
            LOGW("secSize(%" PRIu64 ") is too large.", secSize);
            return false;
        }
        char *secNamesBuf = new char[secSize];
        if (secNamesBuf == nullptr) {
            LOGE("Error in ElfFile::ParseSecNamesStr(): new secNamesBuf failed");
            return false;
        }
        (void)memset_s(secNamesBuf, secSize, '\0', secSize);
        if (!Read(secOffset, secNamesBuf, secSize)) {
            delete[] secNamesBuf;
            secNamesBuf = nullptr;
            return false;
        }
        sectionNames_ = std::string(secNamesBuf, secNamesBuf + secSize);
        delete[] secNamesBuf;
        secNamesBuf = nullptr;
    } else {
        LOGE("e_shstrndx(%u) cannot greater than or equal  e_shnum(%u)", ehdr.e_shstrndx, ehdr.e_shnum);
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
        std::string shdrName = GetSectionNameByIndex(shdr.sh_name);
        if (shdrName.empty()) {
            LOGE("Failed to get section name");
            continue;
        }
        ElfShdr elfShdr;
        elfShdr.name = shdr.sh_name;
        elfShdr.type = shdr.sh_type;
        elfShdr.flags = static_cast<uint64_t>(shdr.sh_flags);
        elfShdr.addr = static_cast<uint64_t>(shdr.sh_addr);
        elfShdr.offset = static_cast<uint64_t>(shdr.sh_offset);
        elfShdr.size = static_cast<uint64_t>(shdr.sh_size);
        elfShdr.link = shdr.sh_link;
        elfShdr.info = shdr.sh_info;
        elfShdr.addrAlign = static_cast<uint64_t>(shdr.sh_addralign);
        elfShdr.entSize = static_cast<uint64_t>(shdr.sh_entsize);
        if (shdr.sh_type == SHT_SYMTAB ||
            shdr.sh_type == SHT_DYNSYM ||
            shdr.sh_type == SHT_PROGBITS ||
            shdr.sh_type == SHT_ARM_EXIDX ||
            shdr.sh_type == SHT_STRTAB) {
            elfShdrs_.emplace(shdrName, elfShdr);
        } else if (shdr.sh_type == SHT_NOTE  && shdrName == ".note.gnu.build-id") {
            elfShdrs_.emplace(shdrName, elfShdr);
        }
    }
    return true;
}

template <typename NhdrType>
std::string ElfParse::ReadBuildId(uint64_t buildIdOffset, uint64_t buildIdSz)
{
    uint64_t offset = 0;
    while (offset < buildIdSz) {
        if (buildIdSz - offset < sizeof(NhdrType)) {
            return "";
        }
        NhdrType hdr;
        if (!Read(buildIdOffset + offset, &hdr, sizeof(hdr))) {
            return "";
        }
        offset += sizeof(hdr);
        if (buildIdSz - offset < hdr.n_namesz) {
            return "";
        }
        if (hdr.n_namesz > 0) {
            std::string name(hdr.n_namesz, '\0');
            if (!Read(buildIdOffset + offset, &(name[0]), hdr.n_namesz)) {
                return "";
            }
            // Trim trailing \0 as GNU is stored as a C string in the ELF file.
            if (name.back() == '\0') {
                name.resize(name.size() - 1);
            }

            // Align hdr.n_namesz to next power multiple of 4. See man 5 elf.
            offset += (hdr.n_namesz + 3) & ~3;
            if (name == "GNU" && hdr.n_type == NT_GNU_BUILD_ID) {
                if (buildIdSz - offset < hdr.n_descsz || hdr.n_descsz == 0) {
                    return "";
                }
                std::string buildIdRaw(hdr.n_descsz, '\0');
                if (Read(buildIdOffset + offset, &buildIdRaw[0], hdr.n_descsz)) {
                    return buildIdRaw;
                }
            }
        }
        // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
        offset += (hdr.n_descsz + 3) & ~3;
    }
    return "";
}


bool ElfParse::ParseElfSymbols()
{
    return true;

}

bool ElfParse::ParseSymbols()
{
    return true;
}

std::string ElfParse::GetSectionNameByIndex(const uint32_t nameIndex)
{
    if (nameIndex >= sectionNames_.size()) {
        LOGE("section index(%u) out of range, endIndex %d ", nameIndex, sectionNames_.size() - 1 );
        return "";
    }
    size_t endIndex = sectionNames_.find('\0', nameIndex);
    if (endIndex != std::string::npos) {
        return sectionNames_.substr(nameIndex, endIndex - nameIndex);
    }
    return "";
}

bool ElfParse::FindSection(ElfShdr& shdr, const std::string& secName)
{
    if (elfShdrs_.find(secName) != elfShdrs_.end()) {
        shdr = elfShdrs_[secName];
        return true;
    }
    return false;
}

bool ElfParse32::InitHeaders()
{
    return ParseAllHeaders<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>();
}

std::string ElfParse32::GetBuildId()
{
    ElfShdr shdr;
    if (FindSection(shdr, ".note.gnu.build-id")){
        return ReadBuildId<Elf32_Nhdr>(shdr.offset, shdr.size);
    }
    return " ";
}
bool ElfParse64::InitHeaders()
{
    return ParseAllHeaders<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>();
}

std::string ElfParse64::GetBuildId()
{
    ElfShdr shdr;
    if (FindSection(shdr, ".note.gnu.build-id")){
        return ReadBuildId<Elf64_Nhdr>(shdr.offset, shdr.size);
    }
    return " ";
}



} // namespace HiviewDFX
} // namespace OHOS