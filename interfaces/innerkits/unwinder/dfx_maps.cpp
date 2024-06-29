/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "dfx_maps.h"

#include <algorithm>
#include <cstring>
#include <securec.h>
#if is_mingw
#include "dfx_nonlinux_define.h"
#else
#include <sys/mman.h>
#endif

#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_log.h"
#include "string_printf.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMaps"

inline const std::string GetMapsFile(pid_t pid)
{
    std::string path = "";
    if ((pid == 0) || (pid == getpid())) {
        path = std::string(PROC_SELF_MAPS_PATH);
    } else if (pid > 0) {
        path = StringPrintf("/proc/%d/maps", (int)pid);
    }
    return path;
}
}

std::shared_ptr<DfxMaps> DfxMaps::Create(pid_t pid, bool crash)
{
    std::string path = GetMapsFile(pid);
    if (path == "") {
        return nullptr;
    }
    auto dfxMaps = std::make_shared<DfxMaps>();
    if (!crash) {
        LOGU("Create maps(%s) with not crash, will only parse exec map", path.c_str());
        dfxMaps->EnableOnlyExec(true);
    }
    if (dfxMaps->Parse(pid, path)) {
        return dfxMaps;
    }
    return nullptr;
}

std::shared_ptr<DfxMaps> DfxMaps::Create(const pid_t pid, const std::string& path)
{
    auto dfxMaps = std::make_shared<DfxMaps>();
    if (dfxMaps->Parse(pid, path)) {
        return dfxMaps;
    }
    return nullptr;
}

bool DfxMaps::Create(const pid_t pid, std::vector<std::shared_ptr<DfxMap>>& maps, std::vector<int>& mapIndex)
{
    std::string path = GetMapsFile(pid);
    if (path == "") {
        return false;
    }
    auto dfxMaps = std::make_shared<DfxMaps>();
    dfxMaps->EnableMapIndex(true);
    if (!dfxMaps->Parse(pid, path)) {
        return false;
    }
    maps = dfxMaps->GetMaps();
    mapIndex = dfxMaps->GetMapIndexVec();
    return true;
}

bool DfxMaps::Parse(const pid_t pid, const std::string& path)
{
    if ((pid < 0) || (path == "")) {
        LOGE("param is error");
        return false;
    }

    FILE* fp = nullptr;
    fp = fopen(path.c_str(), "r");
    if (fp == nullptr) {
        LOGE("Failed to open %s, err=%d", path.c_str(), errno);
        return false;
    }

    char mapBuf[PATH_LEN] = {0};
    int fgetCount = 0;
    while (fgets(mapBuf, sizeof(mapBuf), fp) != nullptr) {
        fgetCount++;
        std::shared_ptr<DfxMap> map = DfxMap::Create(mapBuf, sizeof(mapBuf));
        if (map == nullptr) {
            LOGW("Failed to init map info: %s", mapBuf);
            continue;
        }
        FormatMapName(pid, map->name);
        if (IsArkMapItem(map->name)) {
            AddMap(map, enableMapIndex_);
            continue;
        }
        if (map->name == "[stack]") {
            stackBottom_ = static_cast<uintptr_t>(map->begin);
            stackTop_ = static_cast<uintptr_t>(map->end);
        }
        if (onlyExec_ && !map->IsMapExec()) {
            continue;
        }
        if ((!enableMapIndex_) || IsLegalMapItem(map->name, false)) {
            AddMap(map, enableMapIndex_);
        }
    }
    (void)fclose(fp);
    if (fgetCount == 0) {
        LOGE("Failed to get maps(%s), err(%d).", path.c_str(), errno);
        return false;
    }
    size_t mapsSize = GetMapsSize();
    LOGI("parse maps(%s) completed, map size: (%zu), count: (%d)", path.c_str(), mapsSize, fgetCount);
    return mapsSize > 0;
}

void DfxMaps::FormatMapName(pid_t pid, std::string& mapName)
{
    if (pid <= 0 || pid == getpid()) {
        return;
    }
    // format sandbox file path, add '/proc/xxx/root' prefix
    if (StartsWith(mapName, "/data/storage/")) {
        mapName = "/proc/" + std::to_string(pid) + "/root" + mapName;
    }
}

void DfxMaps::UnFormatMapName(std::string& mapName)
{
    // unformat sandbox file path, drop '/proc/xxx/root' prefix
    if (StartsWith(mapName, "/proc/")) {
        auto startPos = mapName.find("/data/storage/");
        if (startPos != std::string::npos) {
            mapName = mapName.substr(startPos);
        }
    }
}

bool DfxMaps::IsArkMapItem(const std::string& name)
{
    if (StartsWith(name, "[anon:ArkTS Code") || EndsWith(name, ".hap") || EndsWith(name, ".hsp")) {
        return true;
    }
    return false;
}

bool DfxMaps::IsLegalMapItem(const std::string& name, bool withArk)
{
    // some special
    if (withArk && IsArkMapItem(name)) {
        return true;
    }
    if (EndsWith(name, "[vdso]")) {
        return true;
    }
    if (name.empty() || name.find(':') != std::string::npos || name.front() == '[' ||
        name.back() == ']' || std::strncmp(name.c_str(), "/dev/", sizeof("/dev/")) == 0 ||
        std::strncmp(name.c_str(), "/memfd:", sizeof("/memfd:")) == 0 ||
        std::strncmp(name.c_str(), "//anon", sizeof("//anon")) == 0 ||
        EndsWith(name, ".ttf") || EndsWith(name, ".ai")) {
        return false;
    }
    return true;
}

void DfxMaps::AddMap(std::shared_ptr<DfxMap> map, bool enableMapIndex)
{
    maps_.emplace_back(map);
    if (enableMapIndex && !maps_.empty()) {
        mapIndex_.emplace_back(maps_.size() - 1);
    }
}

bool DfxMaps::FindMapByAddr(uintptr_t addr, std::shared_ptr<DfxMap>& map) const
{
    if ((maps_.empty()) || (addr == 0x0)) {
        return false;
    }

    size_t first = 0;
    size_t last = maps_.size();
    while (first < last) {
        size_t index = (first + last) / 2;
        const auto& cur = maps_[index];
        if (cur == nullptr) {
            continue;
        }
        if (addr >= cur->begin && addr < cur->end) {
            map = cur;
            if (index > 0) {
                map->prevMap = maps_[index - 1];
            }
            return true;
        } else if (addr < cur->begin) {
            last = index;
        } else {
            first = index + 1;
        }
    }
    return false;
}

bool DfxMaps::FindMapByFileInfo(std::string name, uint64_t offset, std::shared_ptr<DfxMap>& map) const
{
    for (auto &iter : maps_) {
        if (name != iter->name) {
            continue;
        }

        if (offset >= iter->offset && (offset - iter->offset) < (iter->end - iter->begin)) {
            LOGI("Found name: %s, offset 0x%" PRIx64 " in map (%" PRIx64 "-%" PRIx64 " offset 0x%" PRIx64 ")",
                name.c_str(), offset, iter->begin, iter->end, iter->offset);
            map = iter;
            return true;
        }
    }
    return false;
}

bool DfxMaps::FindMapsByName(std::string name, std::vector<std::shared_ptr<DfxMap>>& maps) const
{
    if (maps_.empty()) {
        return false;
    }
    for (auto &iter : maps_) {
        if (EndsWith(iter->name, name)) {
            maps.emplace_back(iter);
        }
    }
    return (maps.size() > 0);
}

void DfxMaps::Sort(bool less)
{
    if (maps_.empty()) {
        return;
    }
    if (less) {
        std::sort(maps_.begin(), maps_.end(),
            [](const std::shared_ptr<DfxMap>& a, const std::shared_ptr<DfxMap>& b) {
            if (a == nullptr) {
                return false;
            } else if (b == nullptr) {
                return true;
            }
            return a->begin < b->begin;
        });
    } else {
        std::sort(maps_.begin(), maps_.end(),
            [](const std::shared_ptr<DfxMap>& a, const std::shared_ptr<DfxMap>& b) {
            if (a == nullptr) {
                return true;
            } else if (b == nullptr) {
                return false;
            }
            return a->begin > b->begin;
        });
    }
}

bool DfxMaps::GetStackRange(uintptr_t& bottom, uintptr_t& top)
{
    if (stackBottom_ == 0 || stackTop_ == 0) {
        return false;
    }
    bottom = stackBottom_;
    top = stackTop_;
    return true;
}

bool DfxMaps::IsArkExecutedMap(uintptr_t addr)
{
    std::shared_ptr<DfxMap> map = nullptr;
    if (!FindMapByAddr(addr, map)) {
        LOGU("%s", "Not mapped map for current addr.");
        return false;
    }
    return map->IsArkExecutable();
}
} // namespace HiviewDFX
} // namespace OHOS
