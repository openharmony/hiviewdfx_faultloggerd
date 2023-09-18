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

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxElf"
}

std::shared_ptr<DfxElf> DfxElf::Create(const std::string& path)
{
    auto elf = std::make_shared<DfxElf>(path);
    if (elf->IsValid()) {
        return elf;
    }
    return nullptr;
}

bool DfxElf::Init(const std::string& file)
{
    if (mmap_ == nullptr) {
        mmap_ = std::make_shared<DfxMmap>();
        return mmap_->Init(file);
    }
    return true;
}

void DfxElf::Clear()
{
    if (elfParse_ == nullptr) {
        elfParse_.reset();
        elfParse_ = nullptr;
    }

    if (mmap_ != nullptr) {
        mmap_->Clear();
        mmap_.reset();
        mmap_ = nullptr;
    }
}

bool DfxElf::ParseElfIdent()
{
    uintptr_t curOffset = 0;
    // ELF Magic Number，7f 45 4c 46
    uint8_t ident[SELFMAG + 1];
    if (mmap_->Read(curOffset, ident, SELFMAG) != SELFMAG) {
        return false;
    }

    if (memcmp(ident, ELFMAG, SELFMAG) != 0) {
        return false;
    }

    curOffset += EI_CLASS;
    if (mmap_->Read(curOffset, &classType_, sizeof(uint8_t)) != sizeof(uint8_t)) {
        return false;
    }
    return true;
}

bool DfxElf::InitHeaders()
{
    if (elfParse_ != nullptr) {
        return true;
    }

    if (!ParseElfIdent()) {
        DFXLOG_WARN("ParseElfIdent failed");
        return false;
    }

    if (classType_ == ELFCLASS32) {
        elfParse_ = std::unique_ptr<ElfParser>(new ElfParser32(mmap_));
    } else if (classType_ == ELFCLASS64) {
        elfParse_ = std::unique_ptr<ElfParser>(new ElfParser64(mmap_));
    } else {
        DFXLOG_WARN("InitHeaders failed, classType: %d", classType_);
        return false;
    }
    if (elfParse_ != nullptr) {
        valid_ = true;
        elfParse_->InitHeaders();
    }
    return valid_;
}

bool DfxElf::IsValid()
{
    if (valid_ == false) {
        InitHeaders();
    }
    return valid_;
}

uint8_t DfxElf::GetClassType()
{
    if (IsValid()) {
        return classType_;
    }
    return ELFCLASSNONE;
}

ArchType DfxElf::GetArchType()
{
    if (IsValid()) {
        elfParse_->GetArchType();
    }
    return ARCH_UNKNOWN;
}

int64_t DfxElf::GetLoadBias()
{
    if (loadBias_ == 0) {
        if (IsValid()) {
            loadBias_ = elfParse_->GetLoadBias();
        }
    }
    return loadBias_;
}

uint64_t DfxElf::GetLoadBase(uint64_t mapStart, uint64_t mapOffset)
{
    if (loadBase_ == static_cast<uint64_t>(-1)) {
        if (IsValid()) {
            loadBase_ = mapStart - mapOffset - GetLoadBias();
        }
    }
    return loadBase_;
}

uint64_t DfxElf::GetStartPc()
{
    if (startPc_ == static_cast<uint64_t>(-1)) {
        if (IsValid()) {
            auto startVaddr = elfParse_->GetStartVaddr();
            if (loadBase_ != static_cast<uint64_t>(-1) && startVaddr != static_cast<uint64_t>(-1)) {
                startPc_ = startVaddr + loadBase_;
            }
        }
    }
    return startPc_;
}

uint64_t DfxElf::GetEndPc()
{
    if (endPc_ == 0) {
        if (IsValid()) {
            auto endVaddr = elfParse_->GetEndVaddr();
            if (loadBase_ != static_cast<uint64_t>(-1) && endVaddr != 0) {
                endPc_ = endVaddr + loadBase_;
            }
        }
    }
    return endPc_;
}

uint64_t DfxElf::GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset)
{
    return (pc - GetLoadBase(mapStart, mapOffset));
}

uint64_t DfxElf::GetPcAdjustment(uint64_t relPc)
{
#if defined(__arm__)
    if (!IsValid()) {
        return 2;
    }

    if (relPc < static_cast<uint64_t>(GetLoadBias())) {
        if (relPc < 2) {
            return 0;
        }
        return 2;
    }

    uint64_t relPcAdjusted = relPc - GetLoadBias();
    if (relPcAdjusted < 5) {
        if (relPcAdjusted < 2) {
            return 0;
        }
        return 2;
    }
    if (relPcAdjusted & 1) {
        // This is a thumb instruction, it could be 2 or 4 bytes.
        uint32_t value;
        if (!Read((uintptr_t)(relPcAdjusted - 5), &value, sizeof(value)) ||
            (value & 0xe000f000) != 0xe000f000) {
            return 2;
        }
    }
    return 4;
#elif defined(__aarch64__)
    if (relPc <= 4) {
        return 0;
    }
    return 4;
#elif defined(__x86_64__)
    if (relPc < 1) {
        return 0;
    }
    return 1;
#else
#error "Unsupported architecture"
#endif
}

uint64_t DfxElf::GetElfSize()
{
    if (!IsValid()) {
        return 0;
    }
    return elfParse_->GetElfSize();
}

std::string DfxElf::GetElfName()
{
    if (!IsValid()) {
        return "";
    }
    return elfParse_->GetElfName();
}

std::string DfxElf::GetBuildId()
{
    if (buildId_.empty()) {
        if (!IsValid()) {
            return "";
        }
        ShdrInfo shdr;
        if (GetSectionInfo(shdr, NOTE_GNU_BUILD_ID)){
            std::string buildIdHex = GetBuildId((uint64_t)((char *)mmap_->Get() + shdr.offset), shdr.size);
            buildId_ = ToReadableBuildId(buildIdHex);
        }
    }
    return buildId_;
}

std::string DfxElf::GetBuildId(uint64_t noteAddr, uint64_t noteSize)
{
    uint64_t tmp;
    if (__builtin_add_overflow(noteAddr, noteSize, &tmp)) {
        LOGE("noteAddr overflow");
        return "";
    }

    uint64_t offset = 0;
    uint64_t ptr = noteAddr;
    while (offset < noteSize) {
        ElfW(Nhdr) nhdr;
        if (noteSize - offset < sizeof(nhdr)) {
            return "";
        }

        ptr += offset;
        (void)memcpy_s(&nhdr, sizeof(nhdr), &ptr, sizeof(nhdr));

        offset += sizeof(nhdr);
        if (noteSize - offset < nhdr.n_namesz) {
            return "";
        }
        if (nhdr.n_namesz > 0) {
            std::string name(nhdr.n_namesz, '\0');
            ptr += offset;
            (void)memcpy_s(&(name[0]), nhdr.n_namesz, &ptr, nhdr.n_namesz);
            // Trim trailing \0 as GNU is stored as a C string in the ELF file.
            if (name.back() == '\0') {
                name.resize(name.size() - 1);
            }

            // Align nhdr.n_namesz to next power multiple of 4. See man 5 elf.
            offset += (nhdr.n_namesz + 3) & ~3;
            if (name == "GNU" && nhdr.n_type == NT_GNU_BUILD_ID) {
                if (noteSize - offset < nhdr.n_descsz || nhdr.n_descsz == 0) {
                    return "";
                }
                ptr += offset;
                std::string buildIdRaw(nhdr.n_descsz, '\0');
                (void)memcpy_s(&buildIdRaw[0], nhdr.n_descsz, &ptr, nhdr.n_descsz);
                return buildIdRaw;
            }
        }
        // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
        offset += (nhdr.n_descsz + 3) & ~3;
    }
    return "";
}

uintptr_t DfxElf::GetGlobalPointer()
{
    if (!IsValid()) {
        return 0;
    }
    return elfParse_->GetGlobalPointer();
}

std::string DfxElf::ToReadableBuildId(const std::string& buildIdHex)
{
    if (buildIdHex.empty()) {
        return "";
    }
    static const char HEXTABLE[] = "0123456789ABCDEF";
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

bool DfxElf::GetSectionInfo(ShdrInfo& shdr, const std::string secName)
{
    if (!IsValid()) {
         return false;
    }
    return elfParse_->GetSectionInfo(shdr, secName);
}

const std::vector<ElfSymbol>& DfxElf::GetElfSymbols()
{
    return elfParse_->GetElfSymbols();
}

const std::unordered_map<uint64_t, ElfLoadInfo>& DfxElf::GetPtLoads()
{
    return elfParse_->GetPtLoads();
}

bool DfxElf::Read(uintptr_t pos, void *buf, size_t size)
{
    return elfParse_->Read(pos, buf, size);
}

const uint8_t* DfxElf::GetMmapPtr()
{
    if (mmap_ == nullptr) {
        return nullptr;
    }
    return static_cast<uint8_t *>(mmap_->Get());
}

size_t DfxElf::GetMmapSize()
{
    if (mmap_ == nullptr) {
        return 0;
    }
    return mmap_->Size();
}
} // namespace HiviewDFX
} // namespace OHOS
