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

bool ElfParse::Init(const std::string &file)
{
    mmap_ = std::make_shared<DfxMmap>();
    mmap_->Init(file);
    return true;
}

void ElfParse::Clear()
{
    mmap_->Clear();
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

std::string ElfParse::GetBuildId()
{
    return "";
}

bool ElfParse::GetFuncNameAndOffset(uint64_t pc, std::string* funcName, uint64_t* start, uint64_t* end)
{
    return true;
}

bool ElfParse::IsValid()
{
    if (valid_ == false) {
        valid_ = ParseElfIdent();
    }
    return valid_;
}

uint8_t ElfParse::GetClassType()
{
    if (IsValid()) {
        return classType_;
    }
    return ELFCLASSNONE;
}

ArchType ElfParse::GetArchType()
{
    if (IsValid()) {
        return archType_;
    }
    return ARCH_UNKNOWN;
}

bool ElfParse::ParseElfIdent()
{
    // ELF Magic Number，7f 45 4c 46
    uint8_t ident[SELFMAG + 1];
    if (!Read(0, ident, SELFMAG)) {
        return false;
    }

    if (memcmp(ident, ELFMAG, SELFMAG) != 0) {
        return false;
    }

    if (!Read(EI_CLASS, &classType_, 1)) {
        return false;
    }
    return true;
}

bool ElfParse::InitHeaders()
{
    if (!ParseElfIdent()) {
        DFXLOG_WARN("ParseElfIdent failed");
        return false;
    }

    if (classType_ == ELFCLASS32) {
        return ParseAllHeaders<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>();
    } else if (classType_ == ELFCLASS64) {
        return ParseAllHeaders<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>();
    } else {
        DFXLOG_WARN("InitHeaders failed, classType: %d", classType_);
        return false;
    }
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
        case PT_LOAD: {
            if ((phdr.p_flags & PF_X) == 0) {
                continue;
            }

            ptLoads_[phdr.p_offset] = ElfLoadInfo{phdr.p_offset, phdr.p_vaddr, static_cast<size_t>(phdr.p_memsz)};
            // Only set the load bias from the first executable load header.
            if (firstLoadHeader) {
                loadBias_ = static_cast<int64_t>(static_cast<uint64_t>(phdr.p_vaddr) - phdr.p_offset);
            }
            firstLoadHeader = false;
            break;
        }

        case PT_GNU_EH_FRAME: {
            break;
        }

        if (classType_ == ELFCLASS32) {
            case PT_ARM_EXIDX: {
                break;
            }
        }

        default:
            LOGW("Failed parse phdr.p_type = %u", phdr.p_type);
            return false;
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
        if (Read(shNdxOffset, &shdr, sizeof(shdr))) {
            secOffset = shdr.sh_offset;
            secSize = shdr.sh_size;
            if (secSize > Size()) {
                LOGW("secSize(%" PRIu64 ") is too large.", secSize);
                return false;
            }
        }
    }

    // Skip the first header, it's always going to be NULL.
    offset += ehdr.e_shentsize;
    for (size_t i = 1; i < ehdr.e_shnum; i++, offset += ehdr.e_shentsize) {
        if (!Read(offset, &shdr, sizeof(shdr))) {
            return false;
        }
        // TODO
    }
    return true;
}

template <typename ShdrType>
bool FindSection(ShdrType& shdr, const std::string secName)
{
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
