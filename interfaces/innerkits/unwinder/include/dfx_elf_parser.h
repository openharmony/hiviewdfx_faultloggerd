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
    virtual std::string GetElfName() = 0;
    virtual uintptr_t GetGlobalPointer() = 0;
    virtual const std::vector<ElfSymbol>& GetElfSymbols(bool isFunc, bool isSort) = 0;
    virtual bool GetSectionInfo(ShdrInfo& shdr, const uint32_t idx);
    virtual bool GetSectionInfo(ShdrInfo& shdr, const std::string& secName);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads() {return ptLoads_;}
    bool Read(uintptr_t pos, void *buf, size_t size);

protected:
    size_t MmapSize();
    template <typename EhdrType, typename PhdrType, typename ShdrType>
    bool ParseAllHeaders();
    template <typename EhdrType>
    bool ParseElfHeaders(const EhdrType& ehdr);
    template <typename EhdrType, typename PhdrType>
    bool ParseProgramHeaders(const EhdrType& ehdr);
    template <typename EhdrType, typename ShdrType>
    bool ParseSectionHeaders(const EhdrType& ehdr);
    template <typename SymType>
    bool IsFunc(const SymType sym);
    template <typename SymType>
    bool ParseElfSymbols(bool isFunc, bool isSort);
    template <typename SymType>
    bool ParseElfSymbols(ElfShdr shdr, bool isFunc, bool isSort);
    template <typename SymType>
    bool ParseElfSymbol(ElfShdr shdr, uint32_t idx, bool isFunc, ElfSymbol& elfSymbol);
    template <typename DynType>
    bool ParseElfDynamic();
    template <typename DynType>
    bool ParseElfName();
    bool ParseStrTab(std::string& nameStr, const uint64_t offset, const uint64_t size);
    bool GetSectionNameByIndex(std::string& nameStr, const uint32_t name);

protected:
    std::vector<ElfSymbol> elfSymbols_;
    uint64_t dynamicOffset_ = 0;
    uintptr_t dtPltGotAddr_ = 0;
    uintptr_t dtStrtabAddr_ = 0;
    uintptr_t dtStrtabSize_ = 0;
    uintptr_t dtSonameOffset_ = 0;
    std::string soname_ = "";

private:
    std::shared_ptr<DfxMmap> mmap_;
    ArchType archType_ = ARCH_UNKNOWN;
    uint64_t elfSize_ = 0;
    int64_t loadBias_ = 0;
    uint64_t startVaddr_ = static_cast<uint64_t>(-1);
    uint64_t endVaddr_ = 0;
    std::vector<ElfShdr> symShdrs_;
    std::map<std::pair<uint32_t, const std::string>, ShdrInfo> shdrInfoPairs_;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
    std::string sectionNames_;
};

class ElfParser32 : public ElfParser {
public:
    ElfParser32(const std::shared_ptr<DfxMmap>& mmap) : ElfParser(mmap) {}
    bool InitHeaders() override;
    std::string GetElfName() override;
    uintptr_t GetGlobalPointer() override;
    const std::vector<ElfSymbol>& GetElfSymbols(bool isFunc, bool isSort) override;
};

class ElfParser64 : public ElfParser {
public:
    ElfParser64(const std::shared_ptr<DfxMmap>& mmap) : ElfParser(mmap) {}
    bool InitHeaders() override;
    std::string GetElfName() override;
    uintptr_t GetGlobalPointer() override;
    const std::vector<ElfSymbol>& GetElfSymbols(bool isFunc, bool isSort) override;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
