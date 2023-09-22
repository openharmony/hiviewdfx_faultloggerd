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
    bool ret = false;
    if (mmap_ == nullptr) {
        mmap_ = std::make_shared<DfxMmap>();
        ret = mmap_->Init(file);
    }
    ResetElfTable(eti_);
    hasTableInfo_ = false;
    return ret;
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
            LOGU("Elf loadBias: %llx", (uint64_t)loadBias_);
        }
    }
    return loadBias_;
}

uint64_t DfxElf::GetLoadBase(uint64_t mapStart, uint64_t mapOffset)
{
    if (loadBase_ == static_cast<uint64_t>(-1)) {
        if (IsValid()) {
            LOGU("mapStart: %llx, mapOffset: %llx", (uint64_t)mapStart, (uint64_t)mapOffset);
            loadBase_ = mapStart - mapOffset - GetLoadBias();
            LOGU("Elf loadBase: %llx", (uint64_t)loadBase_);
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
                LOGU("Elf startPc: %llx", (uint64_t)startPc_);
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
                LOGU("Elf endPc: %llx", (uint64_t)endPc_);
            }
        }
    }
    return endPc_;
}

uint64_t DfxElf::GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset)
{
    return (pc - GetLoadBase(mapStart, mapOffset));
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
            if (!buildIdHex.empty()) {
                buildId_ = ToReadableBuildId(buildIdHex);
                LOGU("Elf buildId: %s", buildId_.c_str());
            }
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

        ptr = noteAddr + offset;
        (void)memcpy_s(&nhdr, sizeof(nhdr), (void*)ptr, sizeof(nhdr));
        offset += sizeof(nhdr);

        if (noteSize - offset < nhdr.n_namesz) {
            return "";
        }
        if (nhdr.n_namesz > 0) {
            std::string name(nhdr.n_namesz, '\0');
            ptr = noteAddr + offset;
            (void)memcpy_s(&(name[0]), nhdr.n_namesz, (void*)ptr, nhdr.n_namesz);
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
                std::string buildIdRaw(nhdr.n_descsz, '\0');
                ptr = noteAddr + offset;
                (void)memcpy_s(&buildIdRaw[0], nhdr.n_descsz, (void*)ptr, nhdr.n_descsz);
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

const std::vector<ElfSymbol>& DfxElf::GetElfSymbols(bool isSort)
{
    if (elfSymbols_.empty()) {
        elfSymbols_ = elfParse_->GetElfSymbols(false, isSort);
    }
    return elfSymbols_;
}

const std::vector<ElfSymbol>& DfxElf::GetFuncSymbols(bool isSort)
{
    if (funcSymbols_.empty()) {
        funcSymbols_ = elfParse_->GetElfSymbols(true, isSort);
    }
    return funcSymbols_;
}

bool DfxElf::GetFuncInfo(uint64_t addr, std::string& name, uint64_t& start, uint64_t& size)
{
    auto symbols = GetFuncSymbols(true);
    if (symbols.empty()) {
        return false;
    }
    size_t begin = 0;
    size_t end = symbols.size();
    while (begin < end) {
        size_t mid = begin + (end - begin) / 2;
        const auto& symbol = symbols[mid];
        if (addr < symbol.value) {
            end = mid;
        } else if (addr < (symbol.value + symbol.size)) {
            start = symbol.value;
            size = symbol.size;
            name = symbol.nameStr;
            return true;
        } else {
            begin = mid + 1;
        }
    }
    LOGE("GetFuncInfo error");
    return false;
}

const std::unordered_map<uint64_t, ElfLoadInfo>& DfxElf::GetPtLoads()
{
    return elfParse_->GetPtLoads();
}

void DfxElf::ResetElfTable(struct ElfTableInfo& edi)
{
    (void)memset_s(&edi, sizeof(ElfTableInfo), 0, sizeof(ElfTableInfo));
    edi.diEhHdr.format = -1;
    edi.diDebug.format = -1;
#if defined(__arm__)
    edi.diExidx.format = -1;
#endif
}

bool DfxElf::GetExidxTableInfo(struct UnwindTableInfo& ti, std::shared_ptr<DfxMap> map)
{
#if defined(__arm__)
    uintptr_t loadBase = GetLoadBase(map->begin, map->offset);

    ShdrInfo shdr;
    if (GetSectionInfo(shdr, ARM_EXIDX)) {
        ti.startPc = GetStartPc();
        ti.endPc = GetEndPc();
        LOGU("Exidx startPc: %llx, endPc: %llx", (uint64_t)ti.startPc, (uint64_t)ti.endPc);
        ti.gp = 0;
        ti.format = UNW_INFO_FORMAT_ARM_EXIDX;
        ti.tableData = loadBase + shdr.addr;
        ti.tableLen = shdr.size;
        LOGU("Exidx tableLen: %d, tableData: %llx", (int)ti.tableLen, (uint64_t)ti.tableData);
        return true;
    }
    LOGE("Get elf Exidx section error");
#endif
    return false;
}

bool DfxElf::GetEhHdrTableInfo(struct UnwindTableInfo& ti, std::shared_ptr<DfxMap> map)
{
    uintptr_t loadBase = GetLoadBase(map->begin, map->offset);

    ShdrInfo shdr;
    if (GetSectionInfo(shdr, EH_FRAME_HDR)) {
        ti.startPc = GetStartPc();
        ti.endPc = GetEndPc();
        LOGU("EhHdr startPc: %llx, endPc: %llx", (uint64_t)ti.startPc, (uint64_t)ti.endPc);
        ti.gp = GetGlobalPointer();
        LOGU("Elf mmap ptr: %p", (void*)GetMmapPtr());
        struct DwarfEhFrameHdr* hdr = (struct DwarfEhFrameHdr *) (shdr.offset + (char *) GetMmapPtr());
        if (hdr->version != DW_EH_VERSION) {
            LOGE("Hdr version(%d) error", hdr->version);
            return false;
        }

        uintptr_t ptr = (uintptr_t)(&(hdr->ehFrame));
        LOGU("hdr: %llx, ehFrame: %llx", (uint64_t)hdr, (uint64_t)ptr);
        LOGU("gp: %llx, ehFramePtrEnc: %x, fdeCountEnc: %x", (uint64_t)ti.gp, hdr->ehFramePtrEnc, hdr->fdeCountEnc);
        mmap_->SetDataOffset(ti.gp);
        MAYBE_UNUSED uintptr_t ehFrameStart = mmap_->ReadEncodedValue(ptr, hdr->ehFramePtrEnc);
        uintptr_t fdeCount = mmap_->ReadEncodedValue(ptr, hdr->fdeCountEnc);
        if (fdeCount == 0) {
            LOGE("Hdr no FDEs?");
            return false;
        }
        LOGU("ehFrameStart: %llx, fdeCount: %d", (uint64_t)ehFrameStart, (int)fdeCount);

        ti.format = UNW_INFO_FORMAT_REMOTE_TABLE;
        ti.namePtr = 0;
        ti.tableLen = (fdeCount * sizeof(DwarfTableEntry)) / sizeof(uintptr_t);
        ti.tableData = ((loadBase + shdr.addr) + (ptr - (uintptr_t)hdr));
        ti.segbase = loadBase + shdr.addr;
        LOGU("EhHdr tableLen: %d, tableData: %llx, segbase: %llx",
            (int)ti.tableLen, (uint64_t)ti.tableData, (uint64_t)ti.segbase);
        return true;
    }
    LOGE("Get elf EH_FRAME_HDR section error");
    return false;
}

int DfxElf::FindElfTableInfo(struct ElfTableInfo& eti, uintptr_t pc, std::shared_ptr<DfxMap> map)
{
    if (hasTableInfo_) {
        if (pc >= eti_.startPc && pc < eti_.endPc) {
            eti = eti_;
            LOGU("FindElfTableLocal had found");
            return UNW_ERROR_NONE;
        }
    }

#if defined(__arm__)
    hasTableInfo_ = GetExidxTableInfo(eti.diExidx, map);
#endif
    if (!hasTableInfo_) {
        hasTableInfo_ = GetEhHdrTableInfo(eti.diEhHdr, map);
    }

    if (hasTableInfo_) {
        eti.startPc = GetStartPc();
        eti.endPc = GetEndPc();
        if (pc >= eti.startPc && pc < eti.endPc) {
            eti_ = eti;
            return UNW_ERROR_NONE;
        } else {
            LOGE("pc(%p) is not in elf table info?", (void *)pc);
            return UNW_ERROR_PC_NOT_IN_UNWIND_INFO;
        }
    }
    return UNW_ERROR_NO_UNWIND_INFO;
}

int DfxElf::FindUnwindTableInfo(struct UnwindTableInfo& uti, uintptr_t pc, std::shared_ptr<DfxMap> map)
{
    int ret = UNW_ERROR_NONE;
    ElfTableInfo eti;
    ResetElfTable(eti);
    if ((ret = FindElfTableInfo(eti, pc, map)) != UNW_ERROR_NONE) {
        return ret;
    }

#if defined(__arm__)
    if(eti.diExidx.format != -1) {
        uti = eti.diExidx;
    }
#endif
    if(eti.diEhHdr.format != -1) {
        uti = eti.diEhHdr;
    } else if(eti.diDebug.format != -1) {
        uti = eti.diDebug;
    }
    return ret;
}

int DfxElf::FindUnwindTableLocal(struct UnwindTableInfo& uti, uintptr_t pc)
{
    DlCbData cbData;
    memset_s(&cbData, sizeof(cbData), 0, sizeof(cbData));
    cbData.pc = pc;
    ResetElfTable(cbData.eti);
    int ret = dl_iterate_phdr(DlPhdrCb, &cbData);
    if (ret > 0) {
#if defined(__arm__)
        if(cbData.eti.diExidx.format != -1) {
            uti = cbData.eti.diExidx;
        }
#endif
        if(cbData.eti.diEhHdr.format != -1) {
            uti = cbData.eti.diEhHdr;
        } else if(cbData.eti.diDebug.format != -1) {
            uti = cbData.eti.diDebug;
        }
        return UNW_ERROR_NONE;
    }
    return UNW_ERROR_NO_UNWIND_INFO;
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
    ElfTableInfo* eti = &cbData->eti;
    uintptr_t pc = cbData->pc;
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
            if (pc >= vaddr && pc < vaddr + phdr->p_memsz) {
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
        return 0;
    }
    LOGU("Elf name: %s", info->dlpi_name);

    uintptr_t startPc = pText->p_vaddr + loadBase;
    uintptr_t endPc = startPc + pText->p_memsz;

#if defined(__arm__)
    if (pArmExidx) {
        eti->diExidx.format = UNW_INFO_FORMAT_ARM_EXIDX;
        eti->diExidx.startPc = startPc;
        eti->diExidx.endPc = endPc;
        eti->diExidx.gp = 0;
        eti->diExidx.namePtr = (uintptr_t) info->dlpi_name;
        eti->diExidx.tableData = pArmExidx->p_vaddr + loadBase;
        eti->diExidx.tableLen = pArmExidx->p_memsz;
        LOGU("Exidx tableLen: %d, tableData: %llx", (int)eti->diExidx.tableLen, (uint64_t)eti->diExidx.tableData);
        return 1;
    }
#endif

    if (pEhHdr) {
        hdr = (struct DwarfEhFrameHdr *) (pEhHdr->p_vaddr + loadBase);
    } else {
        LOGW("No .eh_frame_hdr section found");
        ElfW(Addr) ehFrame = FindSection(info, EH_FRAME);
        if (ehFrame != 0) {
            LOGD("using synthetic .eh_frame_hdr section for %s", info->dlpi_name);
            synthHdr.version = DW_EH_VERSION;
            synthHdr.ehFramePtrEnc = DW_EH_PE_absptr |
                ((sizeof(ElfW(Addr)) == 4) ? DW_EH_PE_udata4 : DW_EH_PE_udata8);
            synthHdr.fdeCountEnc = DW_EH_PE_omit;
            synthHdr.tableEnc = DW_EH_PE_omit;
            synthHdr.ehFrame = ehFrame;
            hdr = &synthHdr;
        }
    }
    if (hdr != nullptr) {
        if (pDynamic) {
            ElfW(Dyn) *dyn = (ElfW(Dyn) *)(pDynamic->p_vaddr + loadBase);
            for (; dyn->d_tag != DT_NULL; ++dyn) {
                if (dyn->d_tag == DT_PLTGOT) {
                    eti->diEhHdr.gp = dyn->d_un.d_ptr;
                    break;
                }
            }
        } else {
            eti->diEhHdr.gp = 0;
        }

        if (hdr->version != DW_EH_VERSION) {
            LOGE("Hdr version(%d) error", hdr->version);
            return 0;
        }

        uintptr_t ptr = (uintptr_t)(&(hdr->ehFrame));
        LOGU("hdr: %llx, ehFrame: %llx", (uint64_t)hdr, (uint64_t)ptr);

        LOGU("gp: %llx, ehFramePtrEnc: %x, fdeCountEnc: %x",
            (uint64_t)eti->diEhHdr.gp, hdr->ehFramePtrEnc, hdr->fdeCountEnc);
        DfxMemoryCpy::GetInstance().SetDataOffset(eti->diEhHdr.gp);
        MAYBE_UNUSED uintptr_t ehFrameStart = DfxMemoryCpy::GetInstance().ReadEncodedValue(ptr, hdr->ehFramePtrEnc);
        uintptr_t fdeCount = DfxMemoryCpy::GetInstance().ReadEncodedValue(ptr, hdr->fdeCountEnc);
        LOGU("ehFrameStart: %llx, fdeCount: %d", (uint64_t)ehFrameStart, (int)fdeCount);

        eti->diEhHdr.startPc = startPc;
        eti->diEhHdr.endPc = endPc;
        eti->diEhHdr.format = UNW_INFO_FORMAT_REMOTE_TABLE;
        eti->diEhHdr.namePtr = (uintptr_t) info->dlpi_name;
        eti->diEhHdr.tableLen = (fdeCount * sizeof(DwarfTableEntry)) / sizeof(uintptr_t);
        eti->diEhHdr.tableData = ptr;
        eti->diEhHdr.segbase = (uintptr_t)hdr;
        LOGU("EhHdr tableLen: %d, tableData: %llx, segbase: %llx",
            (int)eti->diEhHdr.tableLen, (uint64_t)eti->diEhHdr.tableData, (uint64_t)eti->diEhHdr.segbase);
        return 1;
    }
    return 0;
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
