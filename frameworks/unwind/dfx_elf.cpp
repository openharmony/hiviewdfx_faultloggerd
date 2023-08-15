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

bool DfxElf::Init(const std::string &file)
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
    }
}

bool DfxElf::ParseElfIdent()
{
    uint64_t curOffset = 0;
    // ELF Magic Number，7f 45 4c 46
    uint8_t ident[SELFMAG + 1];
    if (mmap_->Read(&curOffset, ident, SELFMAG) != SELFMAG) {
        return false;
    }

    if (memcmp(ident, ELFMAG, SELFMAG) != 0) {
        return false;
    }

    curOffset += EI_CLASS;
    if (mmap_->Read(&curOffset, &classType_, sizeof(uint8_t)) != sizeof(uint8_t)) {
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
        elfParse_ = std::unique_ptr<ElfParse>(new ElfParse32(mmap_));
    } else if (classType_ == ELFCLASS64) {
        elfParse_ = std::unique_ptr<ElfParse>(new ElfParse64(mmap_));
    } else {
        DFXLOG_WARN("InitHeaders failed, classType: %d", classType_);
        return false;
    }
    if (elfParse_ != nullptr) {
        elfParse_->InitHeaders();
    }
    return (elfParse_ != nullptr);
}

bool DfxElf::IsValid()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (valid_ == false) {
        valid_ = InitHeaders();
    }
    return valid_;
}

uint8_t DfxElf::GetClassType()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsValid()) {
        return classType_;
    }
    return ELFCLASSNONE;
}

ArchType DfxElf::GetArchType()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsValid()) {
        elfParse_->GetArchType();
    }
    return ARCH_UNKNOWN;
}

std::string DfxElf::GetElfName()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValid()) {
        return "";
    }
    return "";
}

std::string DfxElf::GetBuildId()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValid()) {
        return "";
    }
    return elfParse_->GetBuildId();
}

int64_t DfxElf::GetLoadBias()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValid()) {
        return 0;
    }
    return elfParse_->GetLoadBias();
}
std::string DfxElf::GetReadableBuildId()
{
    std::string buildIdHex = elfParse_->GetBuildId();
    return ParseToReadableBuildId(buildIdHex);
}

std::string DfxElf::ParseToReadableBuildId(const std::string& buildIdHex)
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


bool DfxElf::FindSection(ElfShdr& shdr, const std::string& secName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // if (!IsValid()) {
    //     return false;
    // }
    return elfParse_ ->FindSection(shdr, secName);
}
} // namespace HiviewDFX
} // namespace OHOS
