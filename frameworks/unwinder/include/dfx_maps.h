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

/* This files contains feader file of maps module. */

#ifndef DFX_MAPS_H
#define DFX_MAPS_H

#include <mutex>
#include <string>
#include "dfx_elf.h"

namespace OHOS {
namespace HiviewDFX {
struct DfxElfMap {
    static std::shared_ptr<DfxElfMap> Create(const std::string mapBuf, int size);
    static void PermsToProts(const std::string perms, uint64_t& flags);

    bool IsValidPath();
    std::string PrintMap();
    uint64_t GetRelPc(uint64_t pc);
    uint32_t GetPcAdjustment(uint64_t pc);

    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    uint64_t prots = 0;
    std::string perms = ""; // 5:rwxp
    std::string path = "";
    std::shared_ptr<DfxElf> elf = nullptr;
};

class DfxElfMaps {
public:
    DfxElfMaps() = default;
    ~DfxElfMaps() = default;
    static std::shared_ptr<DfxElfMaps> Create(pid_t pid);
    static std::shared_ptr<DfxElfMaps> CreateFromLocal();
    static std::shared_ptr<DfxElfMaps> Create(const std::string path);

    void AddMap(std::shared_ptr<DfxElfMap> map);
    bool FindMapByAddr(uintptr_t address, std::shared_ptr<DfxElfMap>& map) const;
    const std::vector<std::shared_ptr<DfxElfMap>>& GetMaps() const;

    void Sort(bool less = true);
private:
    std::vector<std::shared_ptr<DfxElfMap>> maps_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
