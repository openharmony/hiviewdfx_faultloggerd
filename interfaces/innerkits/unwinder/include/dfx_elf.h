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

#ifndef DFX_ELF_H
#define DFX_ELF_H

#include <memory>
#include <mutex>
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
    static std::shared_ptr<DfxElf> Create(const std::string& file);
    static std::shared_ptr<DfxElf> CreateFromHap(const std::string& file, std::shared_ptr<DfxMap> prevMap,
                                                 uint64_t& offset);
    explicit DfxElf(const std::string& file);
    explicit DfxElf(const int fd, const size_t elfSz, const off_t offset);
    DfxElf(uint8_t *decompressedData, size_t size);
    ~DfxElf() { Clear(); }

    static bool IsValidElf(const void* ptr);
#if is_ohos
    static size_t GetElfSize(const void* ptr);
#endif

    bool IsValid();
    uint8_t GetClassType();
    ArchType GetArchType();
    uint64_t GetElfSize();
    std::string GetElfName();
    std::string GetBuildId();
    void SetBuildId(const std::string& buildId);
    static std::string GetBuildId(uint64_t noteAddr, uint64_t noteSize);
    uintptr_t GetGlobalPointer();
    int64_t GetLoadBias();
    uint64_t GetLoadBase(uint64_t mapStart, uint64_t mapOffset);
    void SetLoadBase(uint64_t base);
    uint64_t GetStartPc();
    uint64_t GetEndPc();
    uint64_t GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset);
    const uint8_t* GetMmapPtr();
    size_t GetMmapSize();
    bool Read(uintptr_t pos, void *buf, size_t size);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads();
    const std::vector<ElfSymbol>& GetElfSymbols(bool isSort = false);
    const std::vector<ElfSymbol>& GetFuncSymbols(bool isSort = false);
    bool GetFuncInfo(uint64_t addr, ElfSymbol& elfSymbol);
    bool GetSectionInfo(ShdrInfo& shdr, const std::string secName);
    int FindUnwindTableInfo(uintptr_t pc, std::shared_ptr<DfxMap> map, struct UnwindTableInfo& uti);
    static int FindUnwindTableLocal(uintptr_t pc, struct UnwindTableInfo& uti);
    static std::string ToReadableBuildId(const std::string& buildIdHex);
    bool IsEmbeddedElf();
    void EnableMiniDebugInfo();
    void InitEmbeddedElf();
    std::shared_ptr<DfxElf> GetEmbeddedElf();
    std::shared_ptr<MiniDebugInfo> GetMiniDebugInfo();

protected:
    void Init();
    void Clear();
    bool InitHeaders();
#if is_ohos && !is_mingw
    static int DlPhdrCb(struct dl_phdr_info *info, size_t size, void *data);
    static bool FindSection(struct dl_phdr_info *info, const std::string secName, ShdrInfo& shdr);
    static bool FillUnwindTableByEhhdrLocal(struct DwarfEhFrameHdr* hdr, struct UnwindTableInfo* uti);
#endif
    bool FillUnwindTableByEhhdr(struct DwarfEhFrameHdr* hdr, uintptr_t shdrBase, struct UnwindTableInfo* uti);
    static bool FillUnwindTableByExidx(ShdrInfo shdr, uintptr_t loadBase, struct UnwindTableInfo* uti);

private:
    bool valid_ = false;
    uint8_t classType_;
    int64_t loadBias_ = 0;
    uint64_t loadBase_ = static_cast<uint64_t>(-1);
    uint64_t startPc_ = static_cast<uint64_t>(-1);
    uint64_t endPc_ = 0;
    std::string buildId_ = "";
    struct UnwindTableInfo uti_;
    bool hasTableInfo_ = false;
    std::shared_ptr<DfxMmap> mmap_ = nullptr;
    std::unique_ptr<ElfParser> elfParse_ = nullptr;
    std::vector<ElfSymbol> elfSymbols_;
    std::vector<ElfSymbol> funcSymbols_;
    bool enableMiniDebugInfo_ = false;
    std::shared_ptr<DfxElf> embeddedElf_ = nullptr;
    std::shared_ptr<MiniDebugInfo> miniDebugInfo_ = nullptr;
    std::shared_ptr<std::vector<uint8_t>> embeddedElfData_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
