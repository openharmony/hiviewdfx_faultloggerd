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

#include "dfx_elf.h"

#include <cstdlib>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <securec.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
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
#define LOG_TAG "DfxElf"
}

std::string ElfFileInfo::GetNameFromPath(const std::string &path)
{
    size_t pos = path.find_last_of("/");
    return path.substr(pos + 1);
}

bool DfxElfImpl::IsValidPc(uint64_t pc)
{
    if (!ptLoads_.empty()) {
        for (const auto& iter : ptLoads_) {
            uint64_t start = iter.second.tableVaddr;
            uint64_t end = start + iter.second.tableSize;
            if (pc >= start && pc < end) {
                return true;
            }
        }
        return false;
    }
    return false;
}

uint64_t DfxElfImpl::GetRealLoadOffset(uint64_t offset) const
{
    if (!ptLoads_.empty()) {
        for (auto& iter : ptLoads_) {
            if ((iter.second.offset & -PAGE_SIZE) == offset) {
                return offset + (iter.second.tableVaddr - iter.second.offset);
            }
        }
    }
    return offset;
}

void DfxElfImpl::GetMaxSize(uint64_t* size)
{
    ElfW(Ehdr) ehdr;
    if (!ReadElfHeaders(ehdr)) {
        return;
    }

    if (ehdr.e_shnum == 0) {
        return;
    }
    *size = ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum;
}

bool DfxElfImpl::Init()
{
    return ReadAllHeaders();
}

bool DfxElfImpl::ReadAllHeaders()
{
    ElfW(Ehdr) ehdr;
    if (!ReadElfHeaders(ehdr)) {
        DFXLOG_ERROR("%s : Failed to read elf headers.", __func__);
        return false;
    }
    ReadProgramHeaders(ehdr);
    ReadSectionHeaders(ehdr);
    return true;
}

bool DfxElfImpl::ReadElfHeaders(ElfW(Ehdr)& ehdr)
{
    if (!memory_->ReadFully(0, &ehdr, sizeof(ehdr))) {
        return false;
    }
    return true;
}

void DfxElfImpl::ReadProgramHeaders(const ElfW(Ehdr)& ehdr)
{
    uint64_t offset = ehdr.e_phoff;
    bool firstExecLoadHeader = true;
    for (size_t i = 0; i < ehdr.e_phnum; i++, offset += ehdr.e_phentsize) {
        ElfW(Phdr) phdr;
        if (!memory_->ReadFully(offset, &phdr, sizeof(phdr))) {
            DFXLOG_ERROR("%s : Failed to read memory of program headers.", __func__);
            return;
        }

        switch (phdr.p_type) {
        case PT_LOAD: {
            if ((phdr.p_flags & PF_X) == 0) {
                continue;
            }

            ElfLoadInfo elfLoadInfo;
            elfLoadInfo.offset = phdr.p_offset;
            elfLoadInfo.tableVaddr = phdr.p_vaddr;
            elfLoadInfo.tableSize = static_cast<size_t>(phdr.p_memsz);
            ptLoads_[phdr.p_offset] = elfLoadInfo;

            if (firstExecLoadHeader) {
                loadBias_ = static_cast<int64_t>(phdr.p_vaddr) - static_cast<int64_t>(phdr.p_offset);
            }
            firstExecLoadHeader = false;
            break;
        }
        default:
            DFXLOG_WARN("phdr type(%u) error", phdr.p_type);
            break;
        }
    }
}

void DfxElfImpl::ReadSectionHeaders(const ElfW(Ehdr)& ehdr)
{
    uint64_t offset = ehdr.e_shoff;
    uint64_t secOffset = 0;
    uint64_t secSize = 0;
    ElfW(Shdr) shdr;
    if (ehdr.e_shstrndx < ehdr.e_shnum) {
        uint64_t shOffset = offset + ehdr.e_shstrndx * ehdr.e_shentsize;
        if (memory_->ReadFully(shOffset, &shdr, sizeof(shdr))) {
            secOffset = shdr.sh_offset;
            secSize = shdr.sh_size;
        }
    }

    // Skip the first header, it's always going to be NULL.
    offset += ehdr.e_shentsize;

    for (size_t i = 1; i < ehdr.e_shnum; i++, offset += ehdr.e_shentsize) {
        if (!memory_->ReadFully(offset, &shdr, sizeof(shdr))) {
            DFXLOG_ERROR("%s : Failed to read memory of section headers.", __func__);
            return;
        }

        if (shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_DYNSYM) {
            // Need to go get the information about the section that contains the string terminated names.
            ElfW(Shdr) strShdr;
            if (shdr.sh_link >= ehdr.e_shnum || shdr.sh_entsize == 0) {
                continue;
            }
            uint64_t strOffset = ehdr.e_shoff + shdr.sh_link * ehdr.e_shentsize;
            if (!memory_->ReadFully(strOffset, &strShdr, sizeof(strShdr))) {
                continue;
            }
            if (strShdr.sh_type != SHT_STRTAB) {
                continue;
            }
            uint32_t shCount = static_cast<uint32_t>(shdr.sh_size / shdr.sh_entsize);
            for (uint32_t j = 0; j < shCount; j++) {
                ElfW(Sym) sym;
                if (!memory_->ReadFully(shdr.sh_offset + j * shdr.sh_entsize, &sym, sizeof(sym))) {
                    continue;
                }

                SymbolInfo info;
                info.start = sym.st_value;
                info.end = sym.st_value + sym.st_size;
                info.name = sym.st_name;
                info.ndx = sym.st_shndx;
                info.type = sym.st_info;
                symbols_.push_back(info);
            }
        } else if (shdr.sh_type == SHT_NOTE) {
            if (shdr.sh_name < secSize) {
                std::string name;
                if (memory_->ReadString(secOffset + shdr.sh_name, &name, secSize - shdr.sh_name) &&
                    name == ".note.gnu.build-id") {
                    buildIdOffset_ = shdr.sh_offset;
                    buildIdSize_ = shdr.sh_size;
                }
            }
        }
    }
}

bool DfxElfImpl::GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* start, uint64_t* end)
{
    if (symbols_.empty()) {
        return false;
    }

    std::vector<SymbolInfo>::iterator it;
    for (it = symbols_.begin(); it != symbols_.end(); ++it) {
        SymbolInfo& symbol = *it;
        if (symbol.ndx == SHN_UNDEF || ELF32_ST_TYPE(symbol.type) != STT_FUNC) {
            continue;
        }

        if ((addr >= symbol.start) && (addr < symbol.end)) {
            *start = symbol.start;
            *end = symbol.end;
            uint64_t nameAddr = symbol.start + symbol.name;
            if (nameAddr < symbol.end) {
                memory_->ReadString(nameAddr, funcName, static_cast<size_t>(symbol.end - nameAddr));
            }
            return true;
        }
    }
    return false;
}

bool DfxElfImpl::GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* funcOffset)
{
    if (symbols_.empty()) {
        return false;
    }

    std::vector<SymbolInfo>::iterator it;
    for (it = symbols_.begin(); it != symbols_.end(); ++it) {
        SymbolInfo& symbol = *it;
        if (symbol.ndx == SHN_UNDEF || ELF32_ST_TYPE(symbol.type) != STT_FUNC) {
            continue;
        }

        if ((addr >= symbol.start) && (addr < symbol.end)) {
            *funcOffset = addr - symbol.start;
            uint64_t nameAddr = symbol.start + symbol.name;
            if (nameAddr < symbol.end) {
                memory_->ReadString(nameAddr, funcName, static_cast<size_t>(symbol.end - nameAddr));
            }
            return true;
        }
    }
    return false;
}

bool DfxElfImpl::GetGlobalVariableOffset(const std::string& name, uint64_t* offset)
{
    if (symbols_.empty()) {
        return false;
    }

    std::vector<SymbolInfo>::iterator it;
    for (it = symbols_.begin(); it != symbols_.end(); ++it) {
        SymbolInfo& symbol = *it;
        if (symbol.ndx == SHN_UNDEF || ELF32_ST_TYPE(symbol.type) != STT_OBJECT ||
            ELF32_ST_BIND(symbol.type) != STB_GLOBAL) {
            continue;
        }

        uint64_t nameAddr = symbol.start + symbol.name;
        if (nameAddr < symbol.end) {
            std::string tmpName;
            if (memory_->ReadString(nameAddr, &tmpName, static_cast<size_t>(symbol.end - nameAddr))
                && tmpName == name) {
                *offset = symbol.start;
                return true;
            }
        }
    }
    return false;
}

int64_t DfxElfImpl::GetLoadBias()
{
    return loadBias_;
}

std::string DfxElfImpl::GetBuildID()
{
    // Ensure there is no overflow in any of the calculations below.
    uint64_t tmp;
    if (__builtin_add_overflow(buildIdOffset_, buildIdSize_, &tmp)) {
        return "";
    }

    uint64_t offset = 0;
    while (offset < buildIdSize_) {
        ElfW(Nhdr) nhdr;
        if (buildIdSize_ - offset < sizeof(nhdr)) {
            return "";
        }

        if (!memory_->ReadFully(buildIdOffset_ + offset, &nhdr, sizeof(nhdr))) {
            return "";
        }
        offset += sizeof(nhdr);

        if (buildIdSize_ - offset < nhdr.n_namesz) {
            return "";
        }

        if (nhdr.n_namesz > 0) {
            std::string name(nhdr.n_namesz, '\0');
            if (!memory_->ReadFully(buildIdOffset_ + offset, &(name[0]), nhdr.n_namesz)) {
                return "";
            }

            // Trim trailing \0 as GNU is stored as a C string in the ELF file.
            if (name.back() == '\0') {
                name.resize(name.size() - 1);
            }

            offset += ALIGN_VALUE(nhdr.n_namesz, 4); // Align hdr.n_namesz to next power multiple of 4. See man 5 elf.
            if (name == "GNU" && nhdr.n_type == NT_GNU_BUILD_ID) {
                if (buildIdSize_ - offset < nhdr.n_descsz || nhdr.n_descsz == 0) {
                    return "";
                }
                std::string buildId(nhdr.n_descsz, '\0');
                if (memory_->ReadFully(buildIdOffset_ + offset, &buildId[0], nhdr.n_descsz)) {
                    return buildId;
                }
                return "";
            }
        }
        offset += ALIGN_VALUE(nhdr.n_descsz, 4); // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
    }
    return "";
}

bool DfxElf::Init()
{
    if (memory_ == nullptr) {
        return false;
    }

    if (!ReadElfInfo()) {
        DFXLOG_ERROR("%s : Failed to read elf info.", __func__);
        return false;
    }

    elfImpl_ = std::unique_ptr<DfxElfImpl>(new DfxElfImpl(memory_));
    if (elfImpl_ == nullptr) {
        return false;
    }
    valid_ = elfImpl_->Init();
    return valid_;
}

bool DfxElf::ReadElfInfo()
{
    if (memory_ == nullptr) {
        return false;
    }

    if (!IsValidElf(memory_)) {
        return false;
    }

    uint8_t eiClass;
    if (!memory_->ReadFully(EI_CLASS, &eiClass, sizeof(eiClass))) {
        return false;
    }
    class_ = eiClass;

    uint16_t machine;
    if (!memory_->ReadFully(EI_NIDENT + sizeof(uint16_t), &machine, sizeof(machine))) {
        return false;
    }
    machine_ = machine;

    if (class_ == ELFCLASS32) {
        if (machine == EM_ARM) {
            arch_ = ARCH_ARM;
        } else if (machine == EM_386) {
            arch_ = ARCH_X86;
        } else {
            LOGI("32 bit elf that is neither arm nor x86: machine = %d\n", machine);
            return false;
        }
    } else if (class_ == ELFCLASS64) {
        if (machine == EM_AARCH64) {
            arch_ = ARCH_ARM64;
        } else if (machine == EM_X86_64) {
            arch_ = ARCH_X86_64;
        } else {
            LOGI("64 bit elf that is neither aarch64 nor x86_64: machine = %d\n", machine);
            return false;
        }
    }
    return true;
}

bool DfxElf::GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* start, uint64_t* end)
{
    std::lock_guard<std::mutex> guard(lock_);
    if (!valid_) {
        return false;
    }
    return elfImpl_->GetFuncNameAndOffset(addr, funcName, start, end);
}

bool DfxElf::GetFuncNameAndOffset(uint64_t addr, std::string* funcName, uint64_t* funcOffset)
{
    std::lock_guard<std::mutex> guard(lock_);
    if (!valid_) {
        return false;
    }
    return elfImpl_->GetFuncNameAndOffset(addr, funcName, funcOffset);
}

bool DfxElf::GetGlobalVariableOffset(const std::string& name, uint64_t* offset)
{
    std::lock_guard<std::mutex> guard(lock_);
    if (!valid_) {
        return false;
    }

    uint64_t vaddr;
    if (!elfImpl_->GetGlobalVariableOffset(name, &vaddr)) {
        return false;
    }
    return true;
}

int64_t DfxElf::GetLoadBias()
{
    if (!valid_) {
        return 0;
    }
    return elfImpl_->GetLoadBias();
}

std::string DfxElf::GetBuildID()
{
    if (!valid_) {
        return "";
    }
    return elfImpl_->GetBuildID();
}

bool DfxElf::IsValidPc(uint64_t pc)
{
    if (!valid_ || (GetLoadBias() > 0 && pc < static_cast<uint64_t>(GetLoadBias()))) {
        return false;
    }

    if (elfImpl_->IsValidPc(pc)) {
        return true;
    }
    return false;
}

uint64_t DfxElf::GetRealLoadOffset(uint64_t offset) const
{
    if (!valid_) {
        return offset;
    }
    return elfImpl_->GetRealLoadOffset(offset);
}

bool DfxElf::IsValidElf(std::shared_ptr<DfxMemory> memory)
{
    if (memory == nullptr) {
        return false;
    }

    // Verify that this is a valid elf file.
    uint8_t e_ident[SELFMAG + 1];
    if (!memory->ReadFully(0, e_ident, SELFMAG)) {
        return false;
    }

    if (memcmp(e_ident, ELFMAG, SELFMAG) != 0) {
        return false;
    }
    return true;
}

uint64_t DfxElf::GetMaxSize(std::shared_ptr<DfxMemory> memory)
{
    if (memory == nullptr) {
        return 0;
    }

    uint64_t size = 0;
    auto elfImpl = std::make_shared<DfxElfImpl>(memory);
    elfImpl->GetMaxSize(&size);
    return size;
}

std::string DfxElf::GetReadableBuildID(const std::string &buildIdHex)
{
    if (buildIdHex.empty()) {
        return "";
    }
    static const char HEXTABLE[] = "0123456789abcdef";
    static const int HEXLENGTH = 16;
    static const int HEX_EXPAND_PARAM = 2;
    const size_t len = buildIdHex.length();
    std::string buildId(len * HEX_EXPAND_PARAM, '\0');

    for (size_t i = 0; i < len; i++) {
        unsigned int n = buildIdHex[i];
        buildId[i * HEX_EXPAND_PARAM] = HEXTABLE[(n >> 4) % HEXLENGTH]; // 4 : higher 4 bit of uint8
        buildId[i * HEX_EXPAND_PARAM + 1] = HEXTABLE[n % HEXLENGTH];
    }
    return buildId;
}
} // namespace HiviewDFX
} // namespace OHOS
