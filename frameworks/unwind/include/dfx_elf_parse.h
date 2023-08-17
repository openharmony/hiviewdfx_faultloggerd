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
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
static const std::string NOTE_GNU_BUILD_ID = ".note.gnu.build-id";
static const std::string GNU_DEBUGDATA = ".gnu_debugdata";
static const std::string EH_FRAME_HDR = ".eh_frame_hdr";
static const std::string EH_FRAME = ".eh_frame";
static const std::string ARM_EXIDX = ".ARM.exidx";
static const std::string ARM_EXTAB = ".ARM.extab";
static const std::string SHSTRTAB = ".shstrtab";
static const std::string STRTAB = ".strtab";
static const std::string SYMTAB = ".symtab";
static const std::string DYNSYM = ".dynsym";
static const std::string DYNSTR = ".dynstr";
static const std::string PLT = ".plt";

struct ElfLoadInfo {
    uint64_t offset = 0;
    uint64_t tableVaddr = 0;
    size_t tableSize = 0;
};

struct ElfSymbol {
    uint32_t name;
    std::string nameStr;
    unsigned char info;
    unsigned char other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
};

struct ElfShdr {
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

struct ShdrInfo {
    uint64_t vaddr;
    uint64_t size;
    uint64_t offset;
};

class ElfParse {
public:
    ElfParse(const std::shared_ptr<DfxMmap>& mmap) : mmap_(mmap) {}
    virtual ~ElfParse() = default;

    virtual bool InitHeaders() = 0;
    virtual ArchType GetArchType() { return archType_; }
    virtual int64_t GetLoadBias();
    virtual std::string GetElfName() = 0;
    virtual std::string GetBuildId() = 0;
    virtual bool GetElfSymbols(std::vector<ElfSymbol>& elfSymbols) = 0;
    virtual bool FindSection(ElfShdr& shdr, const std::string secName);
    virtual bool GetSectionInfo(ShdrInfo& shdr, const std::string secName);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads() {return ptLoads_;}

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
    template <typename SymType>
    bool ParseElfSymbols(std::vector<ElfSymbol>& elfSymbols);
    template <typename NhdrType>
    std::string ParseBuildId(uint64_t buildIdOffset, uint64_t buildIdSz);
    template <typename DynType>
    std::string ParseElfName();
    bool ParseStrTab(std::string& nameStr, const uint64_t offset, const uint64_t size);
    bool ParseDymStr(const uint32_t link);
    bool GetSectionNameByIndex(std::string& nameStr, const uint32_t name);
    bool GetSymbolNameByIndex(std::string& nameStr, const uint32_t link, const uint32_t name);

private:
    std::shared_ptr<DfxMmap> mmap_;
    ArchType archType_;
    int64_t loadBias_ = 0;
    std::unordered_map<std::string, ElfShdr> elfShdrs_;
    std::map<const std::string, ShdrInfo> shdrInfos_;
    std::unordered_map<uint32_t, std::string> elfShdrIndexs_;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
    std::string sectionNames_;
};

class ElfParse32 : public ElfParse {
public:
    ElfParse32(const std::shared_ptr<DfxMmap>& mmap) : ElfParse(mmap) {}
    bool InitHeaders() override;
    std::string GetBuildId() override;
    std::string GetElfName() override;
    bool GetElfSymbols(std::vector<ElfSymbol>& elfSymbols) override;
};

class ElfParse64 : public ElfParse {
public:
    ElfParse64(const std::shared_ptr<DfxMmap>& mmap) : ElfParse(mmap) {}
    bool InitHeaders() override;
    std::string GetBuildId() override;
    std::string GetElfName() override;
    bool GetElfSymbols(std::vector<ElfSymbol>& elfSymbols) override;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
