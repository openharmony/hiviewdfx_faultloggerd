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
    explicit DfxElf(const std::string& file, uint64_t offset);
    ~DfxElf();

    bool IsValid();
    std::string GetElfName();
    std::string GetBuildId();
    int64_t GetLoadBias();
    bool GetFuncNameAndOffset(uint64_t pc, std::string* funcName, uint64_t* start, uint64_t* end);
private:
    std::mutex mutex_;
    std::unique_ptr<ElfParse> elfParse_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
