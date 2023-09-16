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

namespace OHOS {
namespace HiviewDFX {
struct ElfFileInfo {
    std::string name = "";
    std::string path = "";
    std::string buildId = "";
    std::string hash = "";

    static std::string GetNameFromPath(const std::string &path)
    {
        size_t pos = path.find_last_of("/");
        return path.substr(pos + 1);
    }
};

class DfxElf final {
public:
    static std::shared_ptr<DfxElf> Create(const std::string& file);
    explicit DfxElf(const std::string& file) { Init(file); }
    ~DfxElf() { Clear(); }

    bool IsValid();
    uint8_t GetClassType();
    ArchType GetArchType();
    uint64_t GetElfSize();
    std::string GetElfName();
    std::string GetBuildId();
    static std::string GetBuildId(uint64_t noteAddr, uint64_t noteSize);
    uintptr_t GetGlobalPointer();
    int64_t GetLoadBias();
    uint64_t GetLoadBase(uint64_t mapStart, uint64_t mapOffset);
    uint64_t GetStartPc();
    uint64_t GetEndPc();
    uint64_t GetRelPc(uint64_t pc, uint64_t mapStart, uint64_t mapOffset);
    uint64_t GetPcAdjustment(uint64_t pc);
    const uint8_t* GetMmapPtr();
    size_t GetMmapSize();
    bool Read(uintptr_t pos, void *buf, size_t size);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads();
    const std::vector<ElfSymbol>& GetElfSymbols();
    bool GetSectionInfo(ShdrInfo& shdr, const std::string secName);

	static std::string ToReadableBuildId(const std::string& buildIdHex);

protected:
    bool InitHeaders();
    bool Init(const std::string& file);
    void Clear();
    bool ParseElfIdent();

private:
    bool valid_ = false;
    uint8_t classType_;
    int64_t loadBias_ = 0;
    uint64_t loadBase_ = static_cast<uint64_t>(-1);
    uint64_t startPc_ = static_cast<uint64_t>(-1);
    uint64_t endPc_ = static_cast<uint64_t>(-1);
    std::string buildId_ = "";
    std::shared_ptr<DfxMmap> mmap_;
    std::unique_ptr<ElfParser> elfParse_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
