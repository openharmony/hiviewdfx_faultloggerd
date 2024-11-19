/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include <algorithm>
#include <cstdlib>
#include <fcntl.h>
#include <securec.h>
#include <string>
#if is_mingw
#include "dfx_nonlinux_define.h"
#else
#include <elf.h>
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_instr_statistic.h"
#include "dfx_util.h"
#include "dfx_maps.h"
#include "dfx_trace_dlsym.h"
#include "dwarf_define.h"
#if defined(ENABLE_MINIDEBUGINFO)
#include "dfx_xz_utils.h"
#endif
#include "string_util.h"
#include "unwinder_config.h"

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

std::shared_ptr<DfxElf> DfxElf::CreateFromHap(const std::string& file, std::shared_ptr<DfxMap> prevMap,
                                              uint64_t& offset)
{
    // elf header is in the first mmap area
    // c3840000-c38a6000 r--p 00174000 /data/storage/el1/bundle/entry.hap <- program header
    // c38a6000-c3945000 r-xp 001d9000 /data/storage/el1/bundle/entry.hap <- pc is in this region
    // c3945000-c394b000 r--p 00277000 /data/storage/el1/bundle/entry.hap
    // c394b000-c394c000 rw-p 0027c000 /data/storage/el1/bundle/entry.hap
    if (prevMap == nullptr) {
        DFXLOGE("current hap mapitem has no prev mapitem, maybe pc is wrong?");
        return nullptr;
    }
    if (!StartsWith(file, "/proc") || !EndsWith(file, ".hap")) {
        DFXLOGD("Illegal file path, please check file: %{public}s", file.c_str());
        return nullptr;
    }
    int fd = OHOS_TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY));
    if (fd < 0) {
        DFXLOGE("Failed to open hap file, errno(%{public}d)", errno);
        return nullptr;
    }
    auto fileSize = GetFileSize(fd);
    size_t elfSize = 0;
    size_t size = prevMap->end - prevMap->begin;
    do {
        auto mmap = std::make_shared<DfxMmap>();
        if (!mmap->Init(fd, size, (off_t)prevMap->offset)) {
            DFXLOGE("Failed to mmap program header in hap.");
            break;
        }

        elfSize = GetElfSize(mmap->Get());
        if (elfSize <= 0 || elfSize + prevMap->offset > static_cast<uint64_t>(fileSize)) {
            DFXLOGE("Invalid elf size? elf size: %{public}d, hap size: %{public}d", (int)elfSize, (int)fileSize);
            elfSize = 0;
            break;
        }

        offset -= prevMap->offset;
    } while (false);

    if (elfSize != 0) {
        DFXLOGU("elfSize: %{public}zu", elfSize);
        auto elf = std::make_shared<DfxElf>(fd, elfSize, prevMap->offset);
        if (elf->IsValid()) {
            close(fd);
            elf->SetBaseOffset(prevMap->offset);
            return elf;
        }
    }
    close(fd);
    return nullptr;
}

DfxElf::DfxElf(const std::string& file)
{
    if (mmap_ == nullptr && (!file.empty())) {
        DFXLOGU("file: %{public}s", file.c_str());
#if defined(is_ohos) && is_ohos
        if (!DfxMaps::IsLegalMapItem(file)) {
            DFXLOGD("Illegal map file, please check file: %{public}s", file.c_str());
            return;
        }
#endif
        std::string realPath = file;
        if (!StartsWith(file, "/proc/")) { // sandbox file should not be check by realpath function
            if (!RealPath(file, realPath)) {
                DFXLOGW("Failed to realpath %{public}s, errno(%{public}d)", file.c_str(), errno);
                return;
            }
        }
#if defined(is_mingw) && is_mingw
        int fd = OHOS_TEMP_FAILURE_RETRY(open(realPath.c_str(), O_RDONLY | O_BINARY));
#else
        int fd = OHOS_TEMP_FAILURE_RETRY(open(realPath.c_str(), O_RDONLY));
#endif
        if (fd > 0) {
            auto size = static_cast<size_t>(GetFileSize(fd));
            mmap_ = std::make_shared<DfxMmap>();
            if (!mmap_->Init(fd, size, 0)) {
                DFXLOGE("Failed to mmap init.");
            }
            close(fd);
        } else {
            DFXLOGE("Failed to open file: %{public}s", file.c_str());
        }
    }
    Init();
}

DfxElf::DfxElf(const int fd, const size_t elfSz, const off_t offset)
{
    if (mmap_ == nullptr) {
        mmap_ = std::make_shared<DfxMmap>();
        if (!mmap_->Init(fd, elfSz, offset)) {
            DFXLOGE("Failed to mmap init elf in hap.");
        }
    }
    Init();
}

DfxElf::DfxElf(uint8_t *decompressedData, size_t size)
{
    if (mmap_ == nullptr) {
        mmap_ = std::make_shared<DfxMmap>();
        // this mean the embedded elf initialization.
        mmap_->Init(decompressedData, size);
        mmap_->SetNeedUnmap(false);
    }
    Init();
}

void DfxElf::Init()
{
    uti_.namePtr = 0;
    uti_.format = -1;
    hasTableInfo_ = false;
}

void DfxElf::Clear()
{
    if (elfParse_ != nullptr) {
        elfParse_.reset();
        elfParse_ = nullptr;
    }

    if (mmap_ != nullptr) {
        mmap_->Clear();
        mmap_.reset();
        mmap_ = nullptr;
    }
}

bool DfxElf::IsEmbeddedElfValid()
{
#if defined(ENABLE_MINIDEBUGINFO)
    if (embeddedElf_ == nullptr) {
        return InitEmbeddedElf();
    }
    return embeddedElf_ != nullptr && embeddedElf_->IsValid();
#endif
    return false;
}

std::shared_ptr<DfxElf> DfxElf::GetEmbeddedElf()
{
    return embeddedElf_;
}

std::shared_ptr<MiniDebugInfo> DfxElf::GetMiniDebugInfo()
{
    return miniDebugInfo_;
}

bool DfxElf::InitEmbeddedElf()
{
#if defined(ENABLE_MINIDEBUGINFO)
    DFX_TRACE_SCOPED_DLSYM("InitEmbeddedElf");
    if (!UnwinderConfig::GetEnableMiniDebugInfo() || miniDebugInfo_ == nullptr || GetMmapPtr() == nullptr) {
        return false;
    }
    uint8_t *addr = miniDebugInfo_->offset + const_cast<uint8_t*>(GetMmapPtr());
    embeddedElfData_ = std::make_shared<std::vector<uint8_t>>();
    if (embeddedElfData_ == nullptr) {
        DFXLOGE("Create embeddedElfData failed.");
        return false;
    }
    if (XzDecompress(addr, miniDebugInfo_->size, embeddedElfData_)) {
        // embeddedElfData_ store the decompressed bytes.
        // use these bytes to construct an elf.
        embeddedElf_ = std::make_shared<DfxElf>(embeddedElfData_->data(), embeddedElfData_->size());
        if (embeddedElf_ != nullptr && embeddedElf_->IsValid()) {
            return true;
        } else {
            DFXLOGE("Failed to parse Embedded Elf.");
        }
    } else {
        DFXLOGE("Failed to decompressed .gnu_debugdata seciton.");
    }
#endif
    return false;
}

bool DfxElf::InitHeaders()
{
    if (mmap_ == nullptr) {
        return false;
    }

    if (elfParse_ != nullptr) {
        return true;
    }

    uint8_t ident[SELFMAG + 1];
    if (!Read(0, ident, SELFMAG) || !IsValidElf(ident, SELFMAG)) {
        return false;
    }

    if (!Read(EI_CLASS, &classType_, sizeof(uint8_t))) {
        return false;
    }

    if (classType_ == ELFCLASS32) {
        elfParse_ = std::unique_ptr<ElfParser>(new ElfParser32(mmap_));
    } else if (classType_ == ELFCLASS64) {
        elfParse_ = std::unique_ptr<ElfParser>(new ElfParser64(mmap_));
    } else {
        DFXLOGW("InitHeaders failed, classType: %{public}d", classType_);
        return false;
    }
    if (elfParse_ != nullptr) {
        valid_ = true;
        elfParse_->InitHeaders();
#if defined(ENABLE_MINIDEBUGINFO)
        miniDebugInfo_ = elfParse_->GetMiniDebugInfo();
#endif
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
            DFXLOGU("Elf loadBias: %{public}" PRIx64 "", (uint64_t)loadBias_);
        }
    }
    return loadBias_;
}

uint64_t DfxElf::GetLoadBase(uint64_t mapStart, uint64_t mapOffset)
{
    if (loadBase_ == static_cast<uint64_t>(-1)) {
        if (IsValid()) {
            DFXLOGU("mapStart: %{public}" PRIx64 ", mapOffset: %{public}" PRIx64 "",
                (uint64_t)mapStart, (uint64_t)mapOffset);
            loadBase_ = mapStart - mapOffset - static_cast<uint64_t>(GetLoadBias());
            DFXLOGU("Elf loadBase: %{public}" PRIx64 "", (uint64_t)loadBase_);
        }
    }
    return loadBase_;
}

void DfxElf::SetLoadBase(uint64_t base)
{
    loadBase_ = base;
}

void DfxElf::SetBaseOffset(uint64_t offset)
{
    baseOffset_ = offset;
}

uint64_t DfxElf::GetBaseOffset()
{
    return baseOffset_;
}

uint64_t DfxElf::GetStartPc()
{
    if (startPc_ == static_cast<uint64_t>(-1)) {
        if (IsValid()) {
            auto startVaddr = elfParse_->GetStartVaddr();
            if (loadBase_ != static_cast<uint64_t>(-1) && startVaddr != static_cast<uint64_t>(-1)) {
                startPc_ = startVaddr + loadBase_;
                DFXLOGU("Elf startPc: %{public}" PRIx64 "", (uint64_t)startPc_);
            }
        }
    }
    return startPc_;
}

uint64_t DfxElf::GetStartVaddr()
{
    if (IsValid()) {
        return elfParse_->GetStartVaddr();
    }
    return 0;
}

uint64_t DfxElf::GetEndPc()
{
    if (endPc_ == 0) {
        if (IsValid()) {
            auto endVaddr = elfParse_->GetEndVaddr();
            if (loadBase_ != static_cast<uint64_t>(-1) && endVaddr != 0) {
                endPc_ = endVaddr + loadBase_;
                DFXLOGU("Elf endPc: %{public}" PRIx64 "", (uint64_t)endPc_);
            }
        }
    }
    return endPc_;
}

uint64_t DfxElf::GetEndVaddr()
{
    if (IsValid()) {
        return elfParse_->GetEndVaddr();
    }
    return 0;
}

uint64_t DfxElf::GetStartOffset()
{
    if (IsValid()) {
        return elfParse_->GetStartOffset();
    }
    return 0;
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

void DfxElf::SetBuildId(const std::string& buildId)
{
    buildId_ = buildId;
}

std::string DfxElf::GetBuildId()
{
    if (buildId_.empty()) {
        if (!IsValid()) {
            return "";
        }
        ShdrInfo shdr;
        if ((GetSectionInfo(shdr, NOTE_GNU_BUILD_ID) || GetSectionInfo(shdr, NOTES)) && GetMmapPtr() != nullptr) {
            std::string buildIdHex = GetBuildId((uint64_t)((char*)GetMmapPtr() + shdr.offset), shdr.size);
            if (!buildIdHex.empty()) {
                buildId_ = ToReadableBuildId(buildIdHex);
                DFXLOGU("Elf buildId: %{public}s", buildId_.c_str());
            }
        }
    }
    return buildId_;
}

std::string DfxElf::GetBuildId(uint64_t noteAddr, uint64_t noteSize)
{
    uint64_t tmp;
    if (__builtin_add_overflow(noteAddr, noteSize, &tmp)) {
        DFXLOGE("noteAddr overflow");
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
            if (memcpy_s(&(name[0]), nhdr.n_namesz, reinterpret_cast<void*>(ptr), nhdr.n_namesz) != 0) {
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
            if (memcpy_s(&buildIdRaw[0], nhdr.n_descsz, reinterpret_cast<void*>(ptr), nhdr.n_descsz) != 0) {
                return "";
            }
            return buildIdRaw;
        }
        // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
        offset += (nhdr.n_descsz + 3) & ~3; // 3 : Align the offset to a 4-byte boundary
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

bool DfxElf::GetSectionInfo(ShdrInfo& shdr, const std::string secName)
{
    if (!IsValid()) {
        return false;
    }
    return elfParse_->GetSectionInfo(shdr, secName);
}

bool DfxElf::GetSectionData(unsigned char *buf, uint64_t size, std::string secName)
{
    if (!IsValid()) {
        return false;
    }
    return elfParse_->GetSectionData(buf, size, secName);
}

const std::vector<ElfSymbol>& DfxElf::GetElfSymbols()
{
    if (!elfSymbols_.empty()) {
        return elfSymbols_;
    }
    elfSymbols_ = elfParse_->GetElfSymbols(false);
#if defined(ENABLE_MINIDEBUGINFO)
    if (IsEmbeddedElfValid()) {
        auto symbols = embeddedElf_->elfParse_->GetElfSymbols(false);
        DFXLOGU("Get EmbeddedElf ElfSymbols, size: %{public}zu", symbols.size());
        elfSymbols_.insert(elfSymbols_.end(), symbols.begin(), symbols.end());
    }
#endif
    std::sort(elfSymbols_.begin(), elfSymbols_.end(), [](const ElfSymbol& sym1, const ElfSymbol& sym2) {
        return sym1.value < sym2.value;
    });
    auto pred = [](ElfSymbol a, ElfSymbol b) { return a.value == b.value; };
    elfSymbols_.erase(std::unique(elfSymbols_.begin(), elfSymbols_.end(), pred), elfSymbols_.end());
    elfSymbols_.shrink_to_fit();
    DFXLOGU("GetElfSymbols, size: %{public}zu", elfSymbols_.size());
    return elfSymbols_;
}

const std::vector<ElfSymbol>& DfxElf::GetFuncSymbols()
{
    if (!funcSymbols_.empty()) {
        return funcSymbols_;
    }
    funcSymbols_ = elfParse_->GetElfSymbols(true);
#if defined(ENABLE_MINIDEBUGINFO)
    if (IsEmbeddedElfValid()) {
        auto symbols = embeddedElf_->elfParse_->GetElfSymbols(true);
        DFXLOGU("Get EmbeddedElf FuncSymbols, size: %{public}zu", symbols.size());
        funcSymbols_.insert(funcSymbols_.end(), symbols.begin(), symbols.end());
    }
#endif
    std::sort(funcSymbols_.begin(), funcSymbols_.end(), [](const ElfSymbol& sym1, const ElfSymbol& sym2) {
        return sym1.value < sym2.value;
    });
    auto pred = [](ElfSymbol a, ElfSymbol b) { return a.value == b.value; };
    funcSymbols_.erase(std::unique(funcSymbols_.begin(), funcSymbols_.end(), pred), funcSymbols_.end());
    funcSymbols_.shrink_to_fit();
    DFXLOGU("GetFuncSymbols, size: %{public}zu", funcSymbols_.size());
    return funcSymbols_;
}

bool DfxElf::GetFuncInfoLazily(uint64_t addr, ElfSymbol& elfSymbol)
{
    DFX_TRACE_SCOPED_DLSYM("GetFuncInfoLazily");
    if (FindFuncSymbol(addr, funcSymbols_, elfSymbol)) {
        return true;
    }
    bool findSymbol = false;
#if defined(ENABLE_MINIDEBUGINFO)
    if (IsEmbeddedElfValid() &&
        embeddedElf_->elfParse_->GetElfSymbolByAddr(addr, elfSymbol)) {
        funcSymbols_.emplace_back(elfSymbol);
        findSymbol = true;
    }
#endif

    if (!findSymbol && elfParse_->GetElfSymbolByAddr(addr, elfSymbol)) {
        funcSymbols_.emplace_back(elfSymbol);
        findSymbol = true;
    }

    if (findSymbol) {
        std::sort(funcSymbols_.begin(), funcSymbols_.end(), [](const ElfSymbol& sym1, const ElfSymbol& sym2) {
            return sym1.value < sym2.value;
        });
        auto pred = [](ElfSymbol a, ElfSymbol b) { return a.value == b.value; };
        funcSymbols_.erase(std::unique(funcSymbols_.begin(), funcSymbols_.end(), pred), funcSymbols_.end());
        funcSymbols_.shrink_to_fit();
        DFXLOGU("GetFuncInfoLazily, size: %{public}zu", funcSymbols_.size());
        return true;
    }
    return false;
}

bool DfxElf::GetFuncInfo(uint64_t addr, ElfSymbol& elfSymbol)
{
    if (UnwinderConfig::GetEnableLoadSymbolLazily()) {
        return GetFuncInfoLazily(addr, elfSymbol);
    }

    auto symbols = GetFuncSymbols();
    return FindFuncSymbol(addr, symbols, elfSymbol);
}

bool DfxElf::FindFuncSymbol(uint64_t addr, const std::vector<ElfSymbol>& symbols, ElfSymbol& elfSymbol)
{
    DFX_TRACE_SCOPED_DLSYM("FindFuncSymbol");
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
            elfSymbol = symbol;
            return true;
        } else {
            begin = mid + 1;
        }
    }
    return false;
}

const std::unordered_map<uint64_t, ElfLoadInfo>& DfxElf::GetPtLoads()
{
    return elfParse_->GetPtLoads();
}

bool DfxElf::FillUnwindTableByExidx(ShdrInfo shdr, uintptr_t loadBase, struct UnwindTableInfo* uti)
{
    if (uti == nullptr) {
        return false;
    }
    uti->gp = 0;
    uti->tableData = loadBase + shdr.addr;
    uti->tableLen = shdr.size;
    INSTR_STATISTIC(InstructionEntriesArmExidx, shdr.size, 0);
    uti->format = UNW_INFO_FORMAT_ARM_EXIDX;
    DFXLOGU("[%{public}d]: tableData: %{public}" PRIx64 ", tableLen: %{public}d", __LINE__,
        (uint64_t)uti->tableData, (int)uti->tableLen);
    return true;
}

#if is_ohos && !is_mingw
bool DfxElf::FillUnwindTableByEhhdrLocal(struct DwarfEhFrameHdr* hdr, struct UnwindTableInfo* uti)
{
    if (hdr == nullptr) {
        return false;
    }
    if (hdr->version != DW_EH_VERSION) {
        DFXLOGE("[%{public}d]: version(%{public}d) error", __LINE__, hdr->version);
        return false;
    }

    uintptr_t ptr = (uintptr_t)(&(hdr->ehFrame));
    DFXLOGU("[%{public}d]: hdr: %{public}" PRIx64 ", ehFrame: %{public}" PRIx64 "", __LINE__,
        (uint64_t)hdr, (uint64_t)ptr);

    auto acc = std::make_shared<DfxAccessorsLocal>();
    auto memory = std::make_shared<DfxMemory>(acc);
    DFXLOGU("[%{public}d]: gp: %{public}" PRIx64 ", ehFramePtrEnc: %{public}x, fdeCountEnc: %{public}x", __LINE__,
        (uint64_t)uti->gp, hdr->ehFramePtrEnc, hdr->fdeCountEnc);
    memory->SetDataOffset(uti->gp);
    MAYBE_UNUSED uintptr_t ehFrameStart = memory->ReadEncodedValue(ptr, hdr->ehFramePtrEnc);
    uintptr_t fdeCount = memory->ReadEncodedValue(ptr, hdr->fdeCountEnc);
    DFXLOGU("[%{public}d]: ehFrameStart: %{public}" PRIx64 ", fdeCount: %{public}d", __LINE__,
        (uint64_t)ehFrameStart, (int)fdeCount);

    if (hdr->tableEnc != (DW_EH_PE_datarel | DW_EH_PE_sdata4)) {
        DFXLOGU("[%{public}d]: tableEnc: %{public}x", __LINE__, hdr->tableEnc);
        if (hdr->fdeCountEnc == DW_EH_PE_omit) {
            fdeCount = ~0UL;
        }
        if (hdr->ehFramePtrEnc == DW_EH_PE_omit) {
            DFXLOGE("[%{public}d]: ehFramePtrEnc(%{public}x) error", __LINE__, hdr->ehFramePtrEnc);
            return false;
        }
        uti->isLinear = true;
        uti->tableLen = fdeCount;
        uti->tableData = ehFrameStart;
    } else {
        uti->isLinear = false;
        uti->tableLen = (fdeCount * sizeof(DwarfTableEntry)) / sizeof(uintptr_t);
        uti->tableData = ptr;
        uti->segbase = (uintptr_t)hdr;
    }
    uti->format = UNW_INFO_FORMAT_REMOTE_TABLE;
    DFXLOGU("[%{public}d]: tableData: %{public}" PRIx64 ", tableLen: %{public}d", __LINE__,
        (uint64_t)uti->tableData, (int)uti->tableLen);
    return true;
}
#endif

bool DfxElf::FillUnwindTableByEhhdr(struct DwarfEhFrameHdr* hdr, uintptr_t shdrBase, struct UnwindTableInfo* uti)
{
    if ((hdr == nullptr) || (uti == nullptr)) {
        return false;
    }
    if (hdr->version != DW_EH_VERSION) {
        DFXLOGE("[%{public}d]: version(%{public}d) error", __LINE__, hdr->version);
        return false;
    }
    uintptr_t ptr = (uintptr_t)(&(hdr->ehFrame));
    DFXLOGU("[%{public}d]: hdr: %{public}" PRIx64 ", ehFrame: %{public}" PRIx64 "", __LINE__,
        (uint64_t)hdr, (uint64_t)ptr);

    uti->gp = GetGlobalPointer();
    DFXLOGU("[%{public}d]: gp: %{public}" PRIx64 ", ehFramePtrEnc: %{public}x, fdeCountEnc: %{public}x", __LINE__,
        (uint64_t)uti->gp, hdr->ehFramePtrEnc, hdr->fdeCountEnc);
    mmap_->SetDataOffset(uti->gp);
    auto ptrOffset = ptr - reinterpret_cast<uintptr_t>(GetMmapPtr());
    MAYBE_UNUSED uintptr_t ehFrameStart = mmap_->ReadEncodedValue(ptrOffset, hdr->ehFramePtrEnc);
    uintptr_t fdeCount = mmap_->ReadEncodedValue(ptrOffset, hdr->fdeCountEnc);
    DFXLOGU("[%{public}d]: ehFrameStart: %{public}" PRIx64 ", fdeCount: %{public}d", __LINE__,
        (uint64_t)ehFrameStart, (int)fdeCount);
    ptr = reinterpret_cast<uintptr_t>(GetMmapPtr()) + ptrOffset;

    if (hdr->tableEnc != (DW_EH_PE_datarel | DW_EH_PE_sdata4)) {
        DFXLOGU("[%{public}d]: tableEnc: %{public}x", __LINE__, hdr->tableEnc);
        if (hdr->fdeCountEnc == DW_EH_PE_omit) {
            fdeCount = ~0UL;
        }
        if (hdr->ehFramePtrEnc == DW_EH_PE_omit) {
            DFXLOGE("[%{public}d]: ehFramePtrEnc(%{public}x) error", __LINE__, hdr->ehFramePtrEnc);
            return false;
        }
        uti->isLinear = true;
        uti->tableLen = fdeCount;
        uti->tableData = shdrBase + ehFrameStart;
        uti->segbase = shdrBase;
    } else {
        uti->isLinear = false;
        uti->tableLen = (fdeCount * sizeof(DwarfTableEntry)) / sizeof(uintptr_t);
        uti->tableData = shdrBase + ptr - (uintptr_t)hdr;
        uti->segbase = shdrBase;
    }
    uti->format = UNW_INFO_FORMAT_REMOTE_TABLE;
    DFXLOGU("[%{public}d]: tableData: %{public}" PRIx64 ", tableLen: %{public}d", __LINE__,
        (uint64_t)uti->tableData, (int)uti->tableLen);
    return true;
}

int DfxElf::FindUnwindTableInfo(uintptr_t pc, std::shared_ptr<DfxMap> map, struct UnwindTableInfo& uti)
{
    if (hasTableInfo_ && pc >= uti_.startPc && pc < uti_.endPc) {
        uti = uti_;
        DFXLOGU("FindUnwindTableInfo had found");
        return UNW_ERROR_NONE;
    }
    if (map == nullptr) {
        return UNW_ERROR_INVALID_MAP;
    }
    uintptr_t loadBase = GetLoadBase(map->begin, map->offset);
    uti.startPc = GetStartPc();
    uti.endPc = GetEndPc();
    if (pc < uti.startPc || pc >= uti.endPc) {
        DFXLOGU("Elf startPc: %{public}" PRIx64 ", endPc: %{public}" PRIx64 "",
            (uint64_t)uti.startPc, (uint64_t)uti.endPc);
        return UNW_ERROR_PC_NOT_IN_UNWIND_INFO;
    }

    ShdrInfo shdr;
#if defined(__arm__)
    if (GetSectionInfo(shdr, ARM_EXIDX)) {
        hasTableInfo_ = FillUnwindTableByExidx(shdr, loadBase, &uti);
    }
#endif

    if (!hasTableInfo_) {
        struct DwarfEhFrameHdr* hdr = nullptr;
        struct DwarfEhFrameHdr synthHdr;
        if (GetSectionInfo(shdr, EH_FRAME_HDR) && GetMmapPtr() != nullptr) {
            INSTR_STATISTIC(InstructionEntriesEhFrame, shdr.size, 0);
            hdr = (struct DwarfEhFrameHdr *) (shdr.offset + (char *)GetMmapPtr());
        } else if (GetSectionInfo(shdr, EH_FRAME) && GetMmapPtr() != nullptr) {
            DFXLOGW("[%{public}d]: Elf(%{public}s) no found .eh_frame_hdr section, " \
                "using synthetic .eh_frame section", __LINE__, map->name.c_str());
            INSTR_STATISTIC(InstructionEntriesEhFrame, shdr.size, 0);
            synthHdr.version = DW_EH_VERSION;
            synthHdr.ehFramePtrEnc = DW_EH_PE_absptr |
                ((sizeof(ElfW(Addr)) == 4) ? DW_EH_PE_udata4 : DW_EH_PE_udata8); // 4 : four bytes
            synthHdr.fdeCountEnc = DW_EH_PE_omit;
            synthHdr.tableEnc = DW_EH_PE_omit;
            synthHdr.ehFrame = (ElfW(Addr))(shdr.offset + (char*)GetMmapPtr());
            hdr = &synthHdr;
        }
        uintptr_t shdrBase = static_cast<uintptr_t>(loadBase + shdr.addr);
        hasTableInfo_ = FillUnwindTableByEhhdr(hdr, shdrBase, &uti);
    }

    if (hasTableInfo_) {
        uti_ = uti;
        return UNW_ERROR_NONE;
    }
    return UNW_ERROR_NO_UNWIND_INFO;
}

int DfxElf::FindUnwindTableLocal(uintptr_t pc, struct UnwindTableInfo& uti)
{
#if is_ohos && !is_mingw
    DlCbData cbData;
    (void)memset_s(&cbData, sizeof(cbData), 0, sizeof(cbData));
    cbData.pc = pc;
    cbData.uti.format = -1;
    int ret = dl_iterate_phdr(DlPhdrCb, &cbData);
    if (ret > 0) {
        if (cbData.uti.format != -1) {
            uti = cbData.uti;
            return UNW_ERROR_NONE;
        }
    }
    return UNW_ERROR_NO_UNWIND_INFO;
#else
    return UNW_ERROR_UNSUPPORTED_VERSION;
#endif
}

#if is_ohos && !is_mingw
bool DfxElf::FindSection(struct dl_phdr_info *info, const std::string secName, ShdrInfo& shdr)
{
    if (info == nullptr) {
        return false;
    }
    const char *file = info->dlpi_name;
    if (strlen(file) == 0) {
        file = PROC_SELF_EXE_PATH;
    }

    auto elf = Create(file);
    if (elf == nullptr) {
        return false;
    }

    return elf->GetSectionInfo(shdr, secName);
}

void DfxElf::ParsePhdr(struct dl_phdr_info *info, std::vector<const ElfW(Phdr) *> &pHdrSections, const uintptr_t pc)
{
    const ElfW(Phdr) *phdr = info->dlpi_phdr;
    for (size_t i = 0; i < info->dlpi_phnum && phdr != nullptr; i++, phdr++) {
        switch (phdr->p_type) {
            case PT_LOAD: {
                ElfW(Addr) vaddr = phdr->p_vaddr + info->dlpi_addr;
                if (pc >= vaddr && pc < vaddr + phdr->p_memsz) {
                    // .text section
                    pHdrSections[TEXT] = phdr;
                }
                break;
            }
#if defined(__arm__)
            case PT_ARM_EXIDX: {
                // .arm_exidx section
                pHdrSections[ARMEXIDX] = phdr;
                break;
            }
#endif
            case PT_GNU_EH_FRAME: {
                // .eh_frame_hdr section
                pHdrSections[EHFRAMEHDR] = phdr;
                break;
            }
            case PT_DYNAMIC: {
                // .dynamic section
                pHdrSections[DYNAMIC] = phdr;
                break;
            }
            default:
                break;
        }
    }
}

bool DfxElf::ProccessDynamic(const ElfW(Phdr) *pDynamic, ElfW(Addr) loadBase, UnwindTableInfo *uti)
{
    ElfW(Dyn) *dyn = (ElfW(Dyn) *)(pDynamic->p_vaddr + loadBase);
    if (dyn == nullptr) {
        return false;
    }
    for (; dyn->d_tag != DT_NULL; ++dyn) {
        if (dyn->d_tag == DT_PLTGOT) {
            uti->gp = dyn->d_un.d_ptr;
            break;
        }
    }
    return true;
}

void DfxElf::InitHdr(struct DwarfEhFrameHdr **hdr, struct DwarfEhFrameHdr& synthHdr,
                     struct dl_phdr_info *info, const ElfW(Phdr) *pEhHdr)
{
    if (pEhHdr) {
        INSTR_STATISTIC(InstructionEntriesEhFrame, pEhHdr->p_memsz, 0);
        *hdr = (struct DwarfEhFrameHdr *) (pEhHdr->p_vaddr + info->dlpi_addr);
    } else {
        ShdrInfo shdr;
        if (FindSection(info, EH_FRAME, shdr)) {
            DFXLOGW("[%{public}d]: Elf(%{public}s) no found .eh_frame_hdr section, " \
                "using synthetic .eh_frame section", __LINE__, info->dlpi_name);
            INSTR_STATISTIC(InstructionEntriesEhFrame, shdr.size, 0);
            synthHdr.version = DW_EH_VERSION;
            synthHdr.ehFramePtrEnc = DW_EH_PE_absptr |
                ((sizeof(ElfW(Addr)) == 4) ? DW_EH_PE_udata4 : DW_EH_PE_udata8); // 4 : four bytes
            synthHdr.fdeCountEnc = DW_EH_PE_omit;
            synthHdr.tableEnc = DW_EH_PE_omit;
            synthHdr.ehFrame = (ElfW(Addr))(shdr.addr + info->dlpi_addr);
            *hdr = &synthHdr;
        }
    }
}

int DfxElf::DlPhdrCb(struct dl_phdr_info *info, size_t size, void *data)
{
    struct DlCbData *cbData = (struct DlCbData *)data;
    if ((info == nullptr) || (cbData == nullptr)) {
        return -1;
    }
    UnwindTableInfo* uti = &cbData->uti;
    uintptr_t pc = cbData->pc;
    const int numOfPhdrSections = 4;
    std::vector<const ElfW(Phdr) *> pHdrSections(numOfPhdrSections, nullptr);
    ParsePhdr(info, pHdrSections, pc);

    if (pHdrSections[TEXT] == nullptr) {
        return 0;
    }
    ElfW(Addr) loadBase = info->dlpi_addr;
    uti->startPc = pHdrSections[TEXT]->p_vaddr + loadBase;
    uti->endPc = uti->startPc + pHdrSections[TEXT]->p_memsz;
    DFXLOGU("Elf name: %{public}s", info->dlpi_name);
    uti->namePtr = (uintptr_t) info->dlpi_name;

#if defined(__arm__)
    if (pHdrSections[ARMEXIDX]) {
        ShdrInfo shdr;
        shdr.addr = pHdrSections[ARMEXIDX]->p_vaddr;
        shdr.size = pHdrSections[ARMEXIDX]->p_memsz;
        return FillUnwindTableByExidx(shdr, loadBase, uti);
    }
#endif

    if (pHdrSections[DYNAMIC]) {
        if (!ProccessDynamic(pHdrSections[DYNAMIC], loadBase, uti)) {
            return 0;
        }
    } else {
        uti->gp = 0;
    }

    struct DwarfEhFrameHdr *hdr = nullptr;
    struct DwarfEhFrameHdr synthHdr;
    InitHdr(&hdr, synthHdr, info, pHdrSections[EHFRAMEHDR]);

    return FillUnwindTableByEhhdrLocal(hdr, uti);
}
#endif

bool DfxElf::Read(uintptr_t pos, void *buf, size_t size)
{
    if ((mmap_ != nullptr) && (mmap_->Read(pos, buf, size) == size)) {
        return true;
    }
    return false;
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

bool DfxElf::IsValidElf(const void* ptr, size_t size)
{
    if (ptr == nullptr) {
        return false;
    }

    if (memcmp(ptr, ELFMAG, size) != 0) {
        DFXLOGD("Invalid elf hdr?");
        return false;
    }
    return true;
}

size_t DfxElf::GetElfSize(const void* ptr)
{
    if (!IsValidElf(ptr, SELFMAG)) {
        return 0;
    }

    const uint8_t* data = static_cast<const uint8_t*>(ptr);
    uint8_t classType = data[EI_CLASS];
    if (classType == ELFCLASS32) {
        Elf32_Ehdr *ehdr = (Elf32_Ehdr *)data;
        return static_cast<size_t>(ehdr->e_shoff + (ehdr->e_shentsize * ehdr->e_shnum));
    } else if (classType == ELFCLASS64) {
        Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
        return static_cast<size_t>(ehdr->e_shoff + (ehdr->e_shentsize * ehdr->e_shnum));
    }
    DFXLOGW("classType(%{public}d) error", classType);
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
