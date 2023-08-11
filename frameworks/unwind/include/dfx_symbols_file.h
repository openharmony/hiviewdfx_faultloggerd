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

#ifndef DFX_SYMBOLS_FILE_H
#define DFX_SYMBOLS_FILE_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "dfx_elf.h"
#include "dfx_symbol.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
static const uint64_t MAX_VADDR = std::numeric_limits<uint64_t>::max();
constexpr const char KERNEL_MMAP_NAME[] = "[kernel.kallsyms]";
constexpr const char KERNEL_MODULES_EXT_NAME[] = ".ko";
constexpr const char KERNEL_ELF_NAME[] = "vmlinux";
constexpr const char MMAP_VDSO_NAME[] = "[vdso]";
constexpr const char MMAP_ANONYMOUS_NAME[] = "[anon]";
constexpr const char MMAP_ANONYMOUS_OHOS_NAME[] = "//anon";

enum SymbolsFileType : uint32_t {
    SYMBOL_KERNEL_FILE,
    SYMBOL_KERNEL_MODULE_FILE,
    SYMBOL_ELF_FILE,
    SYMBOL_HAP_ELF_FILE,
    SYMBOL_FAT_ELF_FILE,
    SYMBOL_JAVA_FILE,
    SYMBOL_JS_FILE,
    SYMBOL_UNKNOW_FILE,
};

class DfxSymbolsFile {
public:
    std::string filePath_ = "";
    SymbolsFileType symbolFileType_;
    int32_t id_ = -1; // used to report protobuf file

    // [14] .text             PROGBITS         00000000002c5000  000c5000
    // min exec addr , general it point to .text
    // we make a default value for min compare
    static const uint64_t maxVaddr = std::numeric_limits<uint64_t>::max();

    uint64_t textExecVaddr_ = maxVaddr;
    uint64_t textExecVaddrFileOffset_ = 0;
    uint64_t textExecVaddrRange_ = maxVaddr;

    DfxSymbolsFile(const std::string path, SymbolsFileType symbolType)
        : filePath_(path), symbolFileType_(symbolType) {}
    virtual ~DfxSymbolsFile() {}

    // set symbols path
    bool SetSymbolsFilePath(const std::string &symbolsSearchPath)
    {
        std::vector<std::string> symbolsSearchPaths = {symbolsSearchPath};
        return SetSymbolsFilePath(symbolsSearchPaths);
    };
    bool SetSymbolsFilePath(const std::vector<std::string> &) { return true; }

    // load symbol from file
    virtual bool LoadSymbols([[maybe_unused]] const std::string &symbolFilePath = "")
    {
        symbolsLoaded_ = true;
        return false;
    };
    // load debug info for unwind
    virtual bool LoadDebugInfo([[maybe_unused]] const std::string &symbolFilePath = "")
    {
        debugInfoLoaded_ = true;
        return false;
    };
    // get the build if from symbols
    const std::string GetBuildId() const { return ""; }

    // get the symbols vector
    const std::vector<DfxSymbol> &GetSymbols() { return symbols_; }
    const std::vector<DfxSymbol *> &GetMatchedSymbols() { return matchedSymbols_; }

    // get vaddr(in symbol) from ip(real addr , after mmap reloc)
    virtual uint64_t GetVaddrInSymbols(uint64_t ip, uint64_t mapStart, uint64_t mapOffset) const { return 0; }

    // get symbols from vaddr
    const DfxSymbol GetSymbolWithVaddr(uint64_t vaddr)
    {
        DfxSymbol symbol;
        return symbol;
    }

    // read the .text section and .eh_frame section (RO) memory from elf mmap
    // unwind use this to check the DWARF and so on
    virtual size_t ReadRoMemory(uint64_t, uint8_t * const, size_t) const
    {
        return 0; // default not support
    }

    // get the section info , like .ARM.exidx
    virtual bool GetSectionInfo([[maybe_unused]] const std::string &name,
                                [[maybe_unused]] uint64_t &sectionVaddr,
                                [[maybe_unused]] uint64_t &sectionSize,
                                [[maybe_unused]] uint64_t &sectionFileOffset) const
    {
        return false;
    }
#ifndef __arm__
    // get hdr info for unwind , need provide the fde table location and entry count
    virtual bool GetHDRSectionInfo([[maybe_unused]] uint64_t &ehFrameHdrElfOffset,
                                   [[maybe_unused]] uint64_t &fdeTableElfOffset,
                                   [[maybe_unused]] uint64_t &fdeTableSize) const
    {
        return false;
    }
#endif

    bool SymbolsLoaded()
    {
        return symbolsLoaded_;
    }

    // this means we are in recording
    // will try read some elf in runtime path
    static bool onRecording_;

protected:
    bool symbolsLoaded_ = false;
    bool debugInfoLoaded_ = false;
    bool debugInfoLoadResult_ = false;
    const std::string FindSymbolFile(const std::vector<std::string> &,
                                     std::string symboleFilePath = "") const { return ""; }

    std::string SearchReadableFile(const std::vector<std::string> &searchPaths,
                                   const std::string &filePath) const {return ""; }
    bool UpdateBuildIdIfMatch(std::string buildId) { return true; }
    std::string buildId_;
    std::vector<std::string> symbolsFileSearchPaths_;
    std::vector<DfxSymbol> symbols_ {};
    std::vector<DfxSymbol *> matchedSymbols_ {};

    void AdjustSymbols() {}
    void SortMatchedSymbols() {}
    bool CheckPathReadable(const std::string &path) const { return true; }
};

class ElfSymbolsFile : public DfxSymbolsFile {
public:
    explicit ElfSymbolsFile(const std::string& file, const SymbolsFileType type = SYMBOL_ELF_FILE)
        : DfxSymbolsFile(file, type) {}
    virtual ~ElfSymbolsFile() {}
};

class HapElfSymbolsFile : public DfxSymbolsFile {
public:
    explicit HapElfSymbolsFile(const std::string& file, const SymbolsFileType type = SYMBOL_ELF_FILE)
        : DfxSymbolsFile(file, type) {}
    virtual ~HapElfSymbolsFile() = default;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
