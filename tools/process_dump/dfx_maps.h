/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "iosfwd"

namespace OHOS {
namespace HiviewDFX {
class DfxElfMap {
public:
    DfxElfMap() = default;
    ~DfxElfMap() = default;
    static std::shared_ptr<DfxElfMap> Create(const std::string mapInfo, int size);
    bool IsValid();
    std::string PrintMap();
    uint64_t GetRelPc(uint64_t pc);
    uint32_t GetPcAdjustment(uint64_t pc);

    uint64_t GetMapBegin() const;
    uint64_t GetMapEnd() const;
    uint64_t GetMapOffset() const;
    std::string GetMapPerms() const;
    std::string GetMapPath() const;
    std::shared_ptr<DfxElf> GetMapImage() const;

    void SetMapBegin(uint64_t begin);
    void SetMapEnd(uint64_t end);
    void SetMapOffset(uint64_t offset);
    void SetMapPerms(const std::string perms, int size);
    void SetMapPath(const std::string path);
    void SetMapImage(std::shared_ptr<DfxElf> elf);

private:
    uint64_t begin_ = 0;
    uint64_t end_ = 0;
    uint64_t offset_ = 0;
    std::string perms_; // 5:rwxp
    std::string path_;
    std::shared_ptr<DfxElf> elf_;
};

class DfxElfMaps {
public:
    DfxElfMaps() = default;
    ~DfxElfMaps() = default;
    static std::shared_ptr<DfxElfMaps> Create(pid_t pid);
    static std::shared_ptr<DfxElfMaps> CreateFromLocal();
    static std::shared_ptr<DfxElfMaps> Create(const std::string path);

    void InsertMapToElfMaps(std::shared_ptr<DfxElfMap> map);
    bool FindMapByPath(const std::string path, std::vector<std::shared_ptr<DfxElfMap>>& maps) const;
    bool FindMapByAddr(uintptr_t address, std::shared_ptr<DfxElfMap>& map) const;

    std::vector<std::shared_ptr<DfxElfMap>> GetValues() const;
    bool CheckPcIsValid(uint64_t pc) const;

    void Sort(void);
private:
    std::vector<std::shared_ptr<DfxElfMap>> maps_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
