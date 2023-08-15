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
#include "dfx_elf_parse.h"

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
    explicit DfxElf(const std::string& file) : file_(file) { Init(); }
    ~DfxElf() { Clear(); }

    std::string GetFilePath() {return file_;}
    bool IsValid();
    uint8_t GetClassType();
    ArchType GetArchType();
    std::string GetElfName();
    std::string GetBuildId();
    bool GetElfSymbols(std::vector<ElfSymbol>& elfSymbols);
    int64_t GetLoadBias();
    bool FindSection(ElfShdr& shdr, const std::string& secName);
    bool GetSectionInfo(ShdrInfo& shdr, const std::string secName);
    const std::unordered_map<uint64_t, ElfLoadInfo>& GetPtLoads();

	static std::string ToReadableBuildId(const std::string& buildIdHex);

protected:
    bool InitHeaders();
    bool Init();
    void Clear();
    bool ParseElfIdent();

private:
    std::mutex mutex_;
    std::string file_;
    bool valid_ = false;
    uint8_t classType_;
    std::string buildId_;
    std::shared_ptr<DfxMmap> mmap_;
    std::unique_ptr<ElfParse> elfParse_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
