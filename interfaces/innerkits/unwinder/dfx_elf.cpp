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
#include "dwarf_define.h"

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
            if (loadBase_ == static_cast<uint64_t>(-1) || startVaddr == static_cast<uint64_t>(-1)) {
                return static_cast<uint64_t>(-1);
            }
            startPc_ = startVaddr + loadBase_;
        }
    }
    return startPc_;
}

uint64_t DfxElf::GetEndPc()
{
    if (endPc_ == static_cast<uint64_t>(-1)) {
        if (IsValid()) {
            auto endVaddr = elfParse_->GetEndVaddr();
            if (loadBase_ == static_cast<uint64_t>(-1) || endVaddr == static_cast<uint64_t>(-1)) {
                return static_cast<uint64_t>(-1);
            }
            endPc_ = endVaddr + loadBase_;
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
    if (!IsValid()) {
         return false;
    }
    return elfParse_->GetSectionInfo(shdr, secName);
}

ElfW(Addr) DfxElf::FindSection(struct dl_phdr_info *info, const std::string secName)
{
    const char *file = info->dlpi_name;
    if (strlen(file) == 0) {
        file = PROC_SELF_EXE_PATH;
    }

    auto elf = Create(file);
    if (elf == nullptr) {
        return 0;
    }

    ElfW(Addr) addr = 0;
    ShdrInfo shdr;
    if (!elf->GetSectionInfo(shdr, secName)) {
        return 0;
    }
    addr = shdr.addr + info->dlpi_addr;
    return addr;
}

int DfxElf::DlPhdrCb(struct dl_phdr_info *info, size_t size, void *data)
{
    struct DlCbData *cbData = (struct DlCbData *)data;
    const ElfW(Phdr) *pText = nullptr;
    const ElfW(Phdr) *pDynamic = nullptr;
#if defined(__arm__)
    const ElfW(Phdr) *pArmExidx = nullptr;
#endif
    const ElfW(Phdr) *pEhHdr = nullptr;
    struct DwarfEhFrameHdr *hdr = nullptr;
    struct DwarfEhFrameHdr synthHdr;
    const ElfW(Phdr) *phdr = info->dlpi_phdr;
    ElfW(Addr) loadBase = info->dlpi_addr, maxLoadAddr = 0;
    for (size_t i = 0; i < info->dlpi_phnum; i++, phdr++) {
        switch (phdr->p_type) {
        case PT_LOAD: {
            ElfW(Addr) vaddr = phdr->p_vaddr + loadBase;
            if (cbData->pc >= vaddr && cbData->pc < vaddr + phdr->p_memsz) {
                pText = phdr;
            }

            if (vaddr + phdr->p_filesz > maxLoadAddr) {
                maxLoadAddr = vaddr + phdr->p_filesz;
            }
            break;
        }
#if defined(__arm__)
        case PT_ARM_EXIDX: {
            pArmExidx = phdr;
            break;
        }
#endif
        case PT_GNU_EH_FRAME: {
            pEhHdr = phdr;
            break;
        }
        case PT_DYNAMIC: {
            pDynamic = phdr;
            break;
        }
        default:
            break;
        }
    }
    if (pText == nullptr) {
        return UNW_ERROR_NO_UNWIND_INFO;
    }

    bool hasTableInfo = false;
#if defined(__arm__)
    if (pArmExidx) {
        cbData->edi.diArm.format = UNW_INFO_FORMAT_ARM_EXIDX;
        cbData->edi.diArm.startPc = pText->p_vaddr + loadBase;
        cbData->edi.diArm.endPc = cbData->edi.diArm.startPc + pText->p_memsz;
        cbData->edi.diArm.namePtr = (uintptr_t) info->dlpi_name;
        cbData->edi.diArm.tableData = pArmExidx->p_vaddr + loadBase;
        cbData->edi.diArm.tableLen = pArmExidx->p_memsz;
        hasTableInfo = true;
    }
#endif

    if (pEhHdr) {
        hdr = (struct DwarfEhFrameHdr *) (pEhHdr->p_vaddr + loadBase);
    } else {
        LOGW("No .eh_frame_hdr section found");
        ElfW(Addr) ehFrame = DfxElf::FindSection(info, EH_FRAME);
        if (ehFrame != 0) {
            LOGD("using synthetic .eh_frame_hdr section for %s", info->dlpi_name);
            synthHdr.version = DW_EH_VERSION;
            synthHdr.ehFramePtrEnc = DW_EH_PE_absptr | ((sizeof(ElfW(Addr)) == 4) ? DW_EH_PE_udata4 : DW_EH_PE_udata8);
            synthHdr.fdeCountEnc = DW_EH_PE_omit;
            synthHdr.tableEnc = DW_EH_PE_omit;
            synthHdr.ehFrame = ehFrame;
            hdr = &synthHdr;
        }
    }
    if (hdr != nullptr) {

    }

    if (hasTableInfo) {
        cbData->edi.startPc = pText->p_vaddr + info->dlpi_addr;
        cbData->edi.endPc = cbData->edi.startPc + pText->p_memsz;
        return UNW_ERROR_NONE;
    }
    return UNW_ERROR_NO_UNWIND_INFO;
}

int DfxElf::ResetElfTableInfo(struct ElfTableInfo& edi)
{
    int ret = memset_s(&edi, sizeof(ElfTableInfo), 0, sizeof(ElfTableInfo));
    edi.diCache.format = -1;
    edi.diDebug.format = -1;
#if defined(__arm__)
    edi.diArm.format = -1;
#endif
    return ret;
}

bool DfxElf::GetElfTableInfo(uintptr_t pc, struct ElfTableInfo& edi)
{
    if (!IsValid()) {
         return false;
    }

    if ((hasTableInfo_) &&
        (elfTableInfo_.startPc <= pc && elfTableInfo_.endPc >= pc)) {
        edi = elfTableInfo_;
        return true;
    }

    ResetElfTableInfo(elfTableInfo_);

    ShdrInfo shdr;
    if (GetSectionInfo(shdr, EH_FRAME_HDR)) {
        elfTableInfo_.diCache.format = UNW_INFO_FORMAT_REMOTE_TABLE;
        elfTableInfo_.diCache.startPc = GetStartPc();
        elfTableInfo_.diCache.endPc = GetEndPc();
        elfTableInfo_.diCache.namePtr = 0;
        //elfTableInfo_.diCache.tableData = shdr.addr;
        //elfTableInfo_.diCache.tableLen = shdr.size;
        hasTableInfo_ = true;
    }

#if defined(__arm__)
    if (GetSectionInfo(shdr, ARM_EXIDX)) {
        elfTableInfo_.diArm.format = UNW_INFO_FORMAT_ARM_EXIDX;
        elfTableInfo_.diArm.startPc = GetStartPc();
        elfTableInfo_.diArm.endPc = GetEndPc();
        elfTableInfo_.diArm.namePtr = 0;
        elfTableInfo_.diArm.tableData = shdr.addr;
        elfTableInfo_.diArm.tableLen = shdr.size;
        hasTableInfo_ = true;
    }
#endif

    if (hasTableInfo_) {
        elfTableInfo_.startPc = GetStartPc();
        elfTableInfo_.endPc = GetEndPc();
        edi = elfTableInfo_;
        return true;
    }
    return false;
}

const std::vector<ElfSymbol>& DfxElf::GetElfSymbols()
{
    return elfParse_->GetElfSymbols();
}

const std::unordered_map<uint64_t, ElfLoadInfo>& DfxElf::GetPtLoads()
{
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

size_t DfxElf::GetMmapSize()
{
    if (mmap_ == nullptr) {
        return 0;
    }
    return mmap_->Size();
}
} // namespace HiviewDFX
} // namespace OHOS
