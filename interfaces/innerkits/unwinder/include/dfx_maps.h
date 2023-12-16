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

#ifndef DFX_MAPS_H
#define DFX_MAPS_H

#include <mutex>
#include <string>
#include <vector>
#include "dfx_map.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMaps {
public:
    DfxMaps() = default;
    ~DfxMaps() = default;
    static const std::string GetMapsFile(pid_t pid = 0);
    static std::shared_ptr<DfxMaps> Create(pid_t pid = 0);
    static bool Create(const pid_t pid, std::vector<std::shared_ptr<DfxMap>>& maps, std::vector<int>& mapIndex);
    static std::shared_ptr<DfxMaps> Create(const pid_t pid, const std::string& path, bool enableMapIndex = false);
    static bool IsLegalMapItem(const std::string& name);

    void AddMap(std::shared_ptr<DfxMap> map, bool enableMapIndex = false);
    void Sort(bool less = true);
    bool FindMapByAddr(uintptr_t addr, std::shared_ptr<DfxMap>& map) const;
    bool FindMapByFileInfo(std::string name, uint64_t offset, std::shared_ptr<DfxMap>& map) const;
    bool FindMapsByName(std::string name, std::vector<std::shared_ptr<DfxMap>>& maps) const;
    const std::vector<std::shared_ptr<DfxMap>>& GetMaps() const { return maps_; }
    const std::vector<int>& GetMapIndexVec() const { return mapIndex_; }
    size_t GetMapsSize() const { return maps_.size(); }
    bool GetStackRange(uintptr_t& bottom, uintptr_t& top);

    bool IsArkExecutedMap(uintptr_t addr);
    uint32_t filePathId_ {0}; // for maps item filePath id
protected:
    std::vector<std::shared_ptr<DfxMap>> maps_;
    std::vector<int> mapIndex_;
private:
    uintptr_t stackBottom_ = 0;
    uintptr_t stackTop_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
