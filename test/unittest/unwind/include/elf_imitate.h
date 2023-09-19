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

#ifndef ELF_PARSER_IMITATE_H
#define ELF_PARSER_IMITATE_H

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
#include "dfx_symbol.h"
#include "dfx_symbols.h"
#include "dfx_elf_define.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class ElfImitate {
public:
    enum class ElfFileType {
        ELF32,
        ELF64,
    };
    ElfImitate()  {}
    ~ElfImitate();

    bool ParseAllHeaders(ElfFileType elfFileType);
    ArchType GetArchType() { return archType_; }
    uint64_t GetElfSize() {return elfSize_; }
    int64_t GetLoadBias() { return loadBias_; }
    uint64_t GetStartVaddr() { return startVaddr_; }
    uint64_t GetEndVaddr() { return endVaddr_; }
    const std::vector<ElfSymbol>& GetElfSymbols();
    bool GetSymSection(ElfShdr& shdr, const std::string secName);
    bool GetSectionInfo(ShdrInfo& shdr, const std::string secName);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads() {return ptLoads_;}
    uint8_t GetClassType() { return classType_;}
    uint64_t GetLoadBase(uint64_t mapStart, uint64_t mapOffset);
    uint64_t GetStartPc();
    uint64_t GetEndPc();
    uint64_t GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset);
    bool ParseSymbols(std::vector<DfxSymbol>& symbols,const std::string& filePath);
    bool AddSymbolsByPlt(std::vector<DfxSymbol>& symbols, const std::string& filePath);
    bool GetFuncNameAndOffset(uint64_t pc, std::string& funcName, uint64_t& start, uint64_t& end);
protected:
    bool ParseElfHeaders();
    bool ParseProgramHeaders(ElfFileType fileType);
    bool ParseSectionHeaders(ElfFileType fileType);
    bool ParseElfSymbols();

protected:
    std::vector<ElfSymbol> elfSymbols_;

private:
    std::vector<std::string> StringSplit(std::string src, const std::string split);
    bool GetMagic(FILE * const fp);
    bool GetClass(FILE * const fp);
    bool GetMachine(FILE * const fp);
    bool GetEntryAddr(FILE * const fp);
    bool GetPrgOffset(FILE * const fp);
    bool GetSecOffset(FILE * const fp);
    bool GetFlag(FILE * const fp);
    bool GetEhdrSize(FILE * const fp);
    bool GetPhdrSize(FILE * const fp);
    bool GetNumPhdrs(FILE * const fp);
    bool GetShdrSize(FILE * const fp);
    bool GetNumShdrs(FILE * const fp);
    bool GetShdrStrTabIdx(FILE * const fp);
    int64_t GetSecIndex(const std::string &line);
    const std::string GetNextShdrLine();
    const std::string GetNextPhdrLine();
    const std::string GetNextSymLine();

    static bool IsFunc(const ElfSymbol symbol);
    std::shared_ptr<DfxMmap> mmap_;
    ArchType archType_ = ARCH_UNKNOWN;
    uint64_t elfSize_ = 0;
    int64_t loadBias_ = 0;
    uint64_t startVaddr_ = static_cast<uint64_t>(-1);
    uint64_t endVaddr_ = 0;
    std::unordered_map<std::string, ElfShdr> symShdrs_;
    std::map<const std::string, ShdrInfo> shdrInfos_;
    std::unordered_map<uint32_t, ElfSecInfo> elfSecInfos_;
    std::unordered_map<uint64_t, ElfLoadInfo> ptLoads_;
    FILE *ehdrFP_ {nullptr};
    FILE *shdrFP_ {nullptr};
    FILE *phdrFP_ {nullptr};
    FILE *symTabFP_ {nullptr};
    unsigned char ehdrIdent_[EI_NIDENT];
    std::string machine_;
    uint8_t classType_;
    uint16_t ehdrSize_;
    uint16_t phdrEntSize_;
    uint16_t phdrNumEnts_;
    uint16_t shdrEntSize_;
    uint16_t shdrNumEnts_;
    uint16_t shdrStrTabIdx_;
    uint32_t ehdrFlags_;
    uint64_t prgEntryVaddr_;
    uint64_t phdrOffset_;
    uint64_t shdrOffset_;
    uint64_t loadBase_ = static_cast<uint64_t>(-1);
    uint64_t startPc_ = static_cast<uint64_t>(-1);
    uint64_t endPc_ = static_cast<uint64_t>(-1);
};
}
}
#endif //ELF_PARSER_IMITATE_H