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
#include <fstream>
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
}

const std::string DfxMaps::GetMapsFile(pid_t pid)
{
    if (pid < 0) {
        return "";
    }
    std::string path;
#if is_ohos
    if ((pid == 0) || (pid == getprocpid())) {
#else
    if ((pid == 0) || (pid == getpid())) {
#endif
        path = std::string(PROC_SELF_MAPS_PATH);
    } else {
        path = StringPrintf("/proc/%d/maps", (int)pid);
    }
    return path;
}

std::shared_ptr<DfxMaps> DfxMaps::Create(pid_t pid)
{
    std::string path = GetMapsFile(pid);
    if (path == "") {
        return nullptr;
    }
    return Create(pid, path);
}

bool DfxMaps::Create(const pid_t pid, std::vector<std::shared_ptr<DfxMap>>& maps, std::vector<int>& mapIndex)
{
    std::string path = GetMapsFile(pid);
    if (path == "") {
        return false;
    }
    auto dfxMaps = Create(pid, path, true);
    if (dfxMaps == nullptr) {
        LOGE("Create maps error, path: %s", path.c_str());
        return false;
    }
    maps = dfxMaps->GetMaps();
    mapIndex = dfxMaps->GetMapIndexVec();
    return true;
}

std::shared_ptr<DfxMaps> DfxMaps::Create(const pid_t pid, const std::string& path, bool enableMapIndex)
{
    std::string realPath = path;
    if (!RealPath(path, realPath)) {
        LOGW("Failed to realpath %s", path.c_str());
        return nullptr;
    }

    std::ifstream ifs;
    ifs.open(realPath, std::ios::in);
    if (ifs.fail()) {
        LOGW("Failed to open %s", realPath.c_str());
        return nullptr;
    }

    auto dfxMaps = std::make_shared<DfxMaps>();
    std::string mapBuf;
    while (getline(ifs, mapBuf)) {
        std::shared_ptr<DfxMap> map = DfxMap::Create(mapBuf, mapBuf.length());
        if (map == nullptr) {
            LOGW("Failed to init map info:%s.", mapBuf.c_str());
            continue;
        } else {
            if (map->name == "[stack]") {
                dfxMaps->stackBottom_ = (uintptr_t)map->begin;
                dfxMaps->stackTop_ = (uintptr_t)map->end;
            }
            FormatMapName(pid, map->name);
            if ((!enableMapIndex) || IsLegalMapItem(map->name)) {
                dfxMaps->AddMap(map, enableMapIndex);
            }
        }
    }
    if (dfxMaps->maps_.empty()) {
        LOGE("DfxMaps size is 0. %s Content :", path.c_str());
        ifs.clear();
        ifs.seekg(0, std::ios::beg);
        std::string tmpBuff;
        while (getline(ifs, tmpBuff)) {
            LOGI("%s", tmpBuff.c_str());
        }
    }
    ifs.close();
    if (!enableMapIndex) {
        dfxMaps->Sort();
    }
    return dfxMaps;
}

void DfxMaps::FormatMapName(pid_t pid, std::string& mapName)
{
    // format sandbox file path, add '/proc/xxx/root' prefix
#if is_ohos
    if (StartsWith(mapName, "/data/storage/") && (pid != getprocpid())) {
#else
    if (StartsWith(mapName, "/data/storage/") && (pid != getpid())) {
#endif
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

bool DfxMaps::IsLegalMapItem(const std::string& name)
{
    // some special
    if (StartsWith(name, "[anon:ArkTS Code") || EndsWith(name, ".hap") || EndsWith(name, ".hsp")) {
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
    if (enableMapIndex) {
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
