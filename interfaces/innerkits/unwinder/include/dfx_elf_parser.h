/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#ifndef DFX_ELF_PARSER_H
#define DFX_ELF_PARSER_H

#include <cstddef>
#if is_mingw
#include "dfx_nonlinux_define.h"
#else
#include <elf.h>
#include <link.h>
#endif
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <set>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "dfx_define.h"
#include "dfx_elf_define.h"
#include "dfx_mmap.h"
#include "dfx_symbol.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class ElfParser {
public:
    ElfParser(const std::shared_ptr<DfxMmap>& mmap) : mmap_(mmap) {}
    virtual ~ElfParser() = default;

    virtual bool InitHeaders() = 0;
    virtual ArchType GetArchType() { return archType_; }
    virtual uint64_t GetElfSize();
    virtual int64_t GetLoadBias() { return loadBias_; }
    virtual uint64_t GetStartVaddr() { return startVaddr_; }
    virtual uint64_t GetEndVaddr() { return endVaddr_; }
    virtual uint64_t GetStartOffset() { return startOffset_; }
    virtual std::string GetElfName() = 0;
    virtual uintptr_t GetGlobalPointer() = 0;
    virtual const std::set<ElfSymbol>& GetFuncSymbols() = 0;
    virtual bool GetSectionInfo(ShdrInfo& shdr, const uint32_t idx);
    virtual bool GetSectionInfo(ShdrInfo& shdr, const std::string& secName);
    virtual bool GetSectionData(unsigned char *buf, uint64_t size, std::string secName);
    virtual bool GetFuncSymbolByAddr(uint64_t addr, ElfSymbol& elfSymbol) = 0;
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads() {return ptLoads_;}
    bool Read(uintptr_t pos, void *buf, size_t size);
    const GnuDebugDataHdr& GetGnuDebugDataHdr() const;
    std::string GetBuildId();
    static std::string ParseHexBuildId(uint64_t noteAddr, uint64_t noteSize);
    std::string GetAdltOriginSoNameByRelPc(uint64_t relPc) const;
    std::string GetNameByNameOffset(uint32_t nameOffset) const;
    const std::string& GetAdltStrtab() const { return adltStrtab_; }
    const std::vector<AdltMapInfo>& GetAdltMap() const { return adltMap_; }
    bool IsAdlt() { return ptAdlt; }

protected:
    size_t MmapSize();
    void GetAdltStrTabSectionInfo(uint64_t shOffset, uint64_t shSize);
    void GetAdltMapSectionInfo(uint64_t shOffset, uint64_t shSize);
    template <typename EhdrType, typename PhdrType, typename ShdrType>
    bool ParseAllHeaders();
    template <typename EhdrType>
    bool ParseElfHeaders(const EhdrType& ehdr);
    template <typename PhdrType>
    void UpdateVaddrAndOffset(const PhdrType& phdr);
    template <typename EhdrType, typename PhdrType>
    bool ParseProgramHeaders(const EhdrType& ehdr);
    template <typename ShdrType>
    void ExtractSymSectionHeadersInfo(const ShdrType& shdr);
    template <typename EhdrType, typename ShdrType>
    bool ExtractSectionHeadersInfo(const EhdrType& ehdr, ShdrType& shdr);
    template <typename EhdrType, typename ShdrType>
    bool ParseSectionHeaders(const EhdrType& ehdr);
    template <typename SymType>
    bool IsFunc(const SymType sym);
    template <typename SymType>
    bool ParseFuncSymbols();
    template <typename SymType>
    bool ParseFuncSymbols(const ElfShdr& shdr);
    template <typename SymType>
    bool ParseFuncSymbolName(const ShdrInfo& linkShdr, SymType sym, std::string& nameStr);
    template <typename SymType>
    bool ParseFuncSymbolByAddr(uint64_t addr, ElfSymbol& elfSymbol);
    template <typename DynType>
    bool ParseElfDynamic();
    template <typename DynType>
    bool ParseElfName();
    bool ParseStrTab(std::string& nameStr, const uint64_t offset, const uint64_t size);
    bool GetSectionNameByIndex(std::string& nameStr, const uint32_t name);
    template <typename SymType>
    bool ReadSymType(const ElfShdr& shdr, const uint32_t idx, SymType& sym);
    std::string ToReadableBuildId(const std::string& buildIdHex);

protected:
    std::set<ElfSymbol> funcSymbols_;
    uint64_t dynamicOffset_ = 0;
    uintptr_t dtPltGotAddr_ = 0;
    uintptr_t dtStrtabAddr_ = 0;
    uintptr_t dtStrtabSize_ = 0;
    uintptr_t dtSonameOffset_ = 0;
    std::string soname_ = "";
    GnuDebugDataHdr gnuDebugDataHdr_;

private:
    std::shared_ptr<DfxMmap> mmap_;
    ArchType archType_ = ARCH_UNKNOWN;
    uint64_t elfSize_ = 0;
    int64_t loadBias_ = 0;
    uint64_t startVaddr_ = static_cast<uint64_t>(-1);
    uint64_t startOffset_ = 0;
    uint64_t endVaddr_ = 0;
    std::vector<ElfShdr> symShdrs_;
    std::map<std::pair<uint32_t, const std::string>, ShdrInfo> shdrInfoPairs_;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
    std::string sectionNames_;
    bool ptAdlt = false;
    std::string adltStrtab_;
    std::vector<AdltMapInfo> adltMap_;
};

class ElfParser32 : public ElfParser {
public:
    ElfParser32(const std::shared_ptr<DfxMmap>& mmap) : ElfParser(mmap) {}
    virtual ~ElfParser32() = default;
    bool InitHeaders() override;
    std::string GetElfName() override;
    uintptr_t GetGlobalPointer() override;
    const std::set<ElfSymbol>& GetFuncSymbols() override;
    bool GetFuncSymbolByAddr(uint64_t addr, ElfSymbol& elfSymbol) override;
};

class ElfParser64 : public ElfParser {
public:
    ElfParser64(const std::shared_ptr<DfxMmap>& mmap) : ElfParser(mmap) {}
    virtual ~ElfParser64() = default;
    bool InitHeaders() override;
    std::string GetElfName() override;
    uintptr_t GetGlobalPointer() override;
    const std::set<ElfSymbol>& GetFuncSymbols() override;
    bool GetFuncSymbolByAddr(uint64_t addr, ElfSymbol& elfSymbol) override;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
