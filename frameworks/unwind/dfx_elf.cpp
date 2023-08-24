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

bool DfxElf::Init()
{
    if (mmap_ == nullptr) {
        mmap_ = std::make_shared<DfxMmap>();
        return mmap_->Init(file_);
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

int64_t DfxElf::GetLoadBias()
{
    if (loadBias_ == 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!IsValid()) {
            return 0;
        }
        loadBias_ = elfParse_->GetLoadBias();
    }
    return loadBias_;
}

uint64_t DfxElf::GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset)
{
    return (pc - mapStart + mapOffset + GetLoadBias());
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
        if (!Read(relPcAdjusted - 5, &value, sizeof(value)) ||
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

uint64_t DfxElf::GetMaxSize()
{
    if (!IsValid()) {
        return 0;
    }
    return elfParse_->GetMaxSize();
}

uint64_t DfxElf::GetStartVaddr()
{
    if (!IsValid()) {
        return 0;
    }
    return elfParse_->GetStartVaddr();
}

uint64_t DfxElf::GetEndVaddr()
{
    if (!IsValid()) {
        return 0;
    }
    return elfParse_->GetEndVaddr();
}

std::string DfxElf::GetElfName()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValid()) {
        return "";
    }
    return elfParse_->GetElfName();
}

std::string DfxElf::GetBuildId()
{
    if (buildId_.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!IsValid()) {
            return "";
        }
        std::string buildIdHex = elfParse_->GetBuildId();
        buildId_ = ToReadableBuildId(buildIdHex);
    }
    return buildId_;
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
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValid()) {
         return false;
    }
    return elfParse_->GetSectionInfo(shdr, secName);
}

bool DfxElf::GetElfDynInfo(uintptr_t pc, struct ElfDynInfo* edi)
{
    if (!IsValid()) {
         return false;
    }

    if ((hasDynInfo_) &&
        (elfDynInfo_.startPc <= pc && elfDynInfo_.endPc >= pc)) {
        *edi = elfDynInfo_;
        return true;
    }

    memset_s(&elfDynInfo_, sizeof(ElfDynInfo), 0, sizeof(ElfDynInfo));
    elfDynInfo_.diCache.format = -1;
    elfDynInfo_.diDebug.format = -1;
#if defined(__arm__)
    elfDynInfo_.diArm.format = -1;
#endif

    ShdrInfo shdr;
    if (GetEhFrameHdrInfo(shdr)) {
        elfDynInfo_.diCache.format = UNW_INFO_FORMAT_REMOTE_TABLE;
        elfDynInfo_.diCache.startPc = GetStartVaddr();
        elfDynInfo_.diCache.endPc = GetEndVaddr();
        elfDynInfo_.diCache.u.rti.namePtr = 0;
        //elfDynInfo_.diCache.u.rti.tableData = shdr.addr;
        //elfDynInfo_.diCache.u.rti.tableLen = shdr.size;
        hasDynInfo_ = true;
    }
#if defined(__arm__)
    if (GetArmExdixInfo(shdr)) {
        elfDynInfo_.diArm.format = UNW_INFO_FORMAT_ARM_EXIDX;
        elfDynInfo_.diArm.startPc = GetStartVaddr();
        elfDynInfo_.diArm.endPc = GetEndVaddr();
        elfDynInfo_.diArm.u.rti.namePtr = 0;
        elfDynInfo_.diArm.u.rti.tableData = shdr.addr;
        elfDynInfo_.diArm.u.rti.tableLen = shdr.size;
        hasDynInfo_ = true;
    }
#endif

    if (hasDynInfo_) {
        elfDynInfo_.startPc = GetStartVaddr();
        elfDynInfo_.endPc = GetEndVaddr();
        *edi = elfDynInfo_;
        return true;
    }
    return false;
}

bool DfxElf::GetArmExdixInfo(ShdrInfo& shdr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValid()) {
         return false;
    }
    return elfParse_->GetArmExdixInfo(shdr);
}

bool DfxElf::GetEhFrameHdrInfo(ShdrInfo& shdr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!IsValid()) {
         return false;
    }
    return elfParse_->GetEhFrameHdrInfo(shdr);
}

const std::vector<ElfSymbol>& DfxElf::GetElfSymbols()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return elfParse_->GetElfSymbols();
}

const std::unordered_map<uint64_t, ElfLoadInfo>& DfxElf::GetPtLoads()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return elfParse_->GetPtLoads();
}

bool DfxElf::Read(uint64_t pos, void *buf, size_t size)
{
    return elfParse_->Read(pos, buf, size);
}

const uint8_t* DfxElf::GetMmap()
{
    if (mmap_ == nullptr) {
        return nullptr;
    }
    return static_cast<uint8_t *>(mmap_->Get());
}
} // namespace HiviewDFX
} // namespace OHOS
