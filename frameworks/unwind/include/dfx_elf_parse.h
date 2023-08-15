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
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
struct ElfLoadInfo {
    uint64_t offset = 0;
    uint64_t tableVaddr = 0;
    size_t tableSize = 0;
};

struct ElfSymbol {
    uint16_t secIndex;
    uint32_t nameIndex;
    uint64_t symValue;
    uint64_t symSize;
};

struct ElfShdr
{
    uint32_t	name;		/* Section name (string tbl index) */
    uint32_t	type;		/* Section type */
    uint64_t	flags;		/* Section flags */
    uint64_t	addr;		/* Section virtual addr at execution */
    uint64_t	offset;		/* Section file offset */
    uint64_t	size;		/* Section size in bytes */
    uint32_t	link;		/* Link to another section */
    uint32_t	info;		/* Additional section information */
    uint64_t	addrAlign;		/* Section alignment */
    uint64_t	entSize;		/* Entry size if section holds table */
};

class ElfParse {
public:
    ElfParse(const std::shared_ptr<DfxMmap>& mmap) : mmap_(mmap) {}
    virtual ~ElfParse() = default;

    virtual bool InitHeaders() = 0;
    virtual ArchType GetArchType() { return archType_; }
    virtual std::string GetElfName();
    virtual int64_t GetLoadBias();
    virtual std::string GetBuildId() = 0;

    bool FindSection(ElfShdr& shdr, const std::string& secName);
    const std::vector<DfxSymbol> &GetSymbols();

protected:
    bool Read(uint64_t pos, void *buf, size_t size);
    size_t Size();

    template <typename EhdrType, typename PhdrType, typename ShdrType>
    bool ParseAllHeaders();
    template <typename EhdrType>
    bool ParseElfHeaders(const EhdrType& ehdr);
    template <typename EhdrType, typename PhdrType>
    bool ParseProgramHeaders(const EhdrType& ehdr);
    template <typename EhdrType, typename ShdrType>
    bool ParseSectionHeaders(const EhdrType& ehdr);
    std::string GetSectionNameByIndex(const uint32_t nameIndex);
    template <typename NhdrType>
    std::string  ReadBuildId(uint64_t buildIdOffset, uint64_t buildIdSz);
    bool ParseElfSymbols();
    bool ParseSymbols();

private:
    std::shared_ptr<DfxMmap> mmap_;
    ArchType archType_;
    int64_t loadBias_ = 0;
    MAYBE_UNUSED std::string buildIdHex_;
    MAYBE_UNUSED std::string buildId_;
    std::vector<ElfSymbol> elfSymbols_;
    std::vector<DfxSymbol> symbols_;
    std::unordered_map<std::string, ElfShdr> elfShdrs_;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
    std::string sectionNames_;
};

class ElfParse32 : public ElfParse {
public:
    ElfParse32(const std::shared_ptr<DfxMmap>& mmap) : ElfParse(mmap) {}
    bool InitHeaders() override;
    std::string GetBuildId() override;
};

class ElfParse64 : public ElfParse {
public:
    ElfParse64(const std::shared_ptr<DfxMmap>& mmap) : ElfParse(mmap) {}
    bool InitHeaders() override;
    std::string GetBuildId() override;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
