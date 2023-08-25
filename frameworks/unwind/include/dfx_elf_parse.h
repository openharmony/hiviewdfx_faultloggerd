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
#ifndef DFX_ELF_PARSE_H
#define DFX_ELF_PARSE_H

#include <cstddef>
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "dfx_define.h"
#include "dfx_mmap.h"
#include "dfx_symbol.h"
#include "elf_define.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class ElfParse {
public:
    ElfParse(const std::shared_ptr<DfxMmap>& mmap) : mmap_(mmap) {}
    virtual ~ElfParse() = default;

    virtual bool InitHeaders() = 0;
    virtual ArchType GetArchType() { return archType_; }
    virtual uint64_t GetMaxSize();
    virtual int64_t GetLoadBias();
    virtual uint64_t GetStartVaddr();
    virtual uint64_t GetEndVaddr();
    virtual std::string GetElfName() = 0;
    virtual std::string GetBuildId() = 0;
    virtual const std::vector<ElfSymbol>& GetElfSymbols() = 0;
    virtual bool GetSymSection(ElfShdr& shdr, const std::string secName);
    virtual bool GetSectionInfo(ShdrInfo& shdr, const std::string secName);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads() {return ptLoads_;}
    bool GetArmExdixInfo(ShdrInfo& shdr);
    bool GetEhFrameHdrInfo(ShdrInfo& shdr);
    bool Read(uint64_t pos, void *buf, size_t size);

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
    bool ParseElfSymbols();
    template <typename NhdrType>
    std::string ParseBuildId(uint64_t noteOffset, uint64_t noteSize);
    template <typename DynType>
    std::string ParseElfName();
    bool ParseStrTab(std::string& nameStr, const uint64_t offset, const uint64_t size);
    bool GetSectionNameByIndex(std::string& nameStr, const uint32_t name);
    const char* GetStrTabPtr(const uint32_t link);

protected:
    std::vector<ElfSymbol> elfSymbols_;

private:
    std::shared_ptr<DfxMmap> mmap_;
    ArchType archType_ = ARCH_UNKNOWN;
    uint64_t maxSize_ = 0;
    int64_t loadBias_ = 0;
    uint64_t startExecVaddr_ = static_cast<uint64_t>(-1);
    uint64_t endExecVaddr_ = 0;
    std::unordered_map<std::string, ElfShdr> symShdrs_;
    std::map<const std::string, ShdrInfo> shdrInfos_;
    std::unordered_map<uint32_t, ElfSecInfo> elfSecInfos_;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
    std::string sectionNames_;
    ShdrInfo armExdixInfo_;
    ShdrInfo ehFrameHdrInfo_;
};

class ElfParse32 : public ElfParse {
public:
    ElfParse32(const std::shared_ptr<DfxMmap>& mmap) : ElfParse(mmap) {}
    bool InitHeaders() override;
    std::string GetBuildId() override;
    std::string GetElfName() override;
    const std::vector<ElfSymbol>& GetElfSymbols() override;
};

class ElfParse64 : public ElfParse {
public:
    ElfParse64(const std::shared_ptr<DfxMmap>& mmap) : ElfParse(mmap) {}
    bool InitHeaders() override;
    std::string GetBuildId() override;
    std::string GetElfName() override;
    const std::vector<ElfSymbol>& GetElfSymbols() override;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
