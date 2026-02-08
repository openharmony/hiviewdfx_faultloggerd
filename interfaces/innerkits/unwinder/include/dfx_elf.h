/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#ifndef DFX_ELF_H
#define DFX_ELF_H

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include "dfx_elf_parser.h"
#include "dfx_map.h"

namespace OHOS {
namespace HiviewDFX {
struct DlCbData {
    uintptr_t pc;
    UnwindTableInfo uti;
    bool singleFde = false;
};

class DfxElf final {
public:
    DfxElf() { Init(); }
    explicit DfxElf (const std::shared_ptr<DfxMmap>& mmap) : mmap_(mmap) { Init(); }
    ~DfxElf() { Clear(); }

    void SetMmap(const std::shared_ptr<DfxMmap>& mmap) { mmap_ = mmap; }
    bool IsValid();
    uint8_t GetClassType();
    ArchType GetArchType();
    uint64_t GetElfSize();
    std::string GetElfName();
    std::string GetBuildId();
    void SetBuildId(const std::string& buildId);
    static std::string GetBuildId(uint64_t noteAddr, uint64_t noteSize);
    GnuDebugDataHdr GetGnuDebugDataHdr();
    uintptr_t GetGlobalPointer();
    int64_t GetLoadBias();
    uint64_t GetLoadBase(uint64_t mapStart, uint64_t mapOffset);
    void SetLoadBase(uint64_t base);
    uint64_t GetStartPc();
    uint64_t GetEndPc();
    uint64_t GetStartVaddr();
    uint64_t GetEndVaddr();
    void SetBaseOffset(uint64_t offset);
    uint64_t GetBaseOffset();
    uint64_t GetStartOffset();
    uint64_t GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset);
    const uint8_t* GetMmapPtr();
    size_t GetMmapSize();
    bool Read(uintptr_t pos, void* buf, size_t size);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads();
    const std::set<ElfSymbol>& GetFuncSymbols();
    bool GetFuncInfo(uint64_t addr, ElfSymbol& elfSymbol);
    bool GetSectionInfo(ShdrInfo& shdr, const std::string secName);
    bool GetSectionData(unsigned char* buf, uint64_t size, std::string secName);
    int FindUnwindTableInfo(uintptr_t pc, std::shared_ptr<DfxMap> map, struct UnwindTableInfo& uti);
    static int FindUnwindTableLocal(uintptr_t pc, struct UnwindTableInfo& uti);
    bool IsAdlt();
    const std::vector<AdltMapInfo>& GetAdltMap() const;
    const std::string& GetAdltStrtab() const;
    std::string GetAdltOriginSoNameByRelPc(uint64_t relPc) const;
    bool FindFuncSymbolByName(std::string funcName, ElfSymbol& elfSymbol);

protected:
    void Init();
    void Clear();
    bool InitHeaders();
    bool IsMiniDebugInfoValid();
#if is_ohos && !is_mingw
    static int DlPhdrCb(struct dl_phdr_info* info, size_t size, void* data);
    static void ParsePhdr(struct dl_phdr_info* info, const ElfW(Phdr)* (&pHdrSections)[4], const uintptr_t pc);
    static bool ProccessDynamic(const ElfW(Phdr)* pDynamic, ElfW(Addr) loadBase, UnwindTableInfo* uti);
    static struct DwarfEhFrameHdr* InitHdr(struct DwarfEhFrameHdr& synthHdr,
        struct dl_phdr_info* info, const ElfW(Phdr)* pEhHdr);
    static bool FindSection(struct dl_phdr_info* info, const std::string secName, ShdrInfo& shdr);
    static bool FillUnwindTableByEhhdrLocal(struct DwarfEhFrameHdr* hdr, struct UnwindTableInfo* uti);
#endif
    bool FillUnwindTableByEhhdr(struct DwarfEhFrameHdr* hdr, uintptr_t shdrBase, struct UnwindTableInfo* uti);
    static bool FillUnwindTableByExidx(ShdrInfo shdr, uintptr_t loadBase, struct UnwindTableInfo* uti);
    bool FindFuncSymbol(uint64_t addr, const std::set<ElfSymbol>& symbols, ElfSymbol& elfSymbol);
    bool IsValidElf(const void* ptr, size_t size);
    bool GetFuncInfoLazily(uint64_t addr, ElfSymbol& elfSymbol);

private:
    bool valid_ = false;
    uint8_t classType_ = 0;
    int64_t loadBias_ = 0;
    uint64_t loadBase_ = static_cast<uint64_t>(-1);
    uint64_t startPc_ = static_cast<uint64_t>(-1);
    uint64_t endPc_ = 0;
    uint64_t baseOffset_ = 0; // use for so in hap
    std::string buildId_ = "";
    struct UnwindTableInfo uti_;
    bool hasTableInfo_ = false;
    std::shared_ptr<DfxMmap> mmap_ = nullptr;
    std::unique_ptr<ElfParser> elfParse_ = nullptr;
    std::set<ElfSymbol> funcSymbols_ {};
    std::shared_ptr<DfxElf> miniDebugInfo_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
