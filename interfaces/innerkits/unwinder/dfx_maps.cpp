/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <securec.h>
#include <sstream>
#if is_mingw
#include "dfx_nonlinux_define.h"
#else
#include <sys/mman.h>
#endif

#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_log.h"
#include "dfx_util.h"
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
    if ((pid == 0) || (pid == getpid())) {
        path = std::string(PROC_SELF_MAPS_PATH);
    } else {
        path = StringPrintf("/proc/%d/maps", pid);
    }
    return path;
}

std::shared_ptr<DfxMaps> DfxMaps::Create(pid_t pid)
{
    std::string path = GetMapsFile(pid);
    if (path == "") {
        return nullptr;
    }
    return Create(path);
}

bool DfxMaps::Create(pid_t pid, std::vector<std::shared_ptr<DfxMap>>& maps, std::vector<int>& mapIndex)
{
    std::string path = GetMapsFile(pid);
    if (path == "") {
        return false;
    }
    auto dfxMaps = Create(path, true);
    if (dfxMaps == nullptr) {
        LOGE("Create maps error, path: %s", path.c_str());
        return false;
    }
    maps = dfxMaps->GetMaps();
    mapIndex = dfxMaps->GetMapIndexVec();
    return true;
}

std::shared_ptr<DfxMaps> DfxMaps::Create(const std::string& path, bool enableMapIndex)
{
    char realPath[PATH_MAX] = {0};
#if is_ohos
    if (realpath(path.c_str(), realPath) == nullptr) {
        DFXLOG_WARN("Maps path(%s) is not exist.", path.c_str());
        return nullptr;
    }
#endif

    std::ifstream ifs;
    ifs.open(realPath, std::ios::in);
    if (ifs.fail()) {
        return nullptr;
    }

    auto dfxMaps = std::make_shared<DfxMaps>();
    std::string mapBuf;
    while (getline(ifs, mapBuf)) {
        std::shared_ptr<DfxMap> map = DfxMap::Create(mapBuf, mapBuf.length());
        if (map == nullptr) {
            DFXLOG_WARN("Failed to init map info:%s.", mapBuf.c_str());
            continue;
        } else {
            if (map->name == "[stack]") {
                dfxMaps->stackBottom_ = (uintptr_t)map->begin;
                dfxMaps->stackTop_ = (uintptr_t)map->end;
            }
            if ((!enableMapIndex) || IsLegalMapItem(map->name)) {
                dfxMaps->AddMap(map, enableMapIndex);
            }
        }
    }
    ifs.close();
    if (!enableMapIndex) {
        dfxMaps->Sort();
    }
    return dfxMaps;
}

bool DfxMaps::IsLegalMapItem(const std::string& name)
{
    // some special
    if (name == "[vdso]") {
        return true;
    }
    if (name.empty() || name.find(':') != std::string::npos || name.front() == '[' ||
        name.back() == ']' || std::strncmp(name.c_str(), "/dev/", sizeof("/dev/")) == 0 ||
        std::strncmp(name.c_str(), "/memfd:", sizeof("/memfd:")) == 0 ||
        std::strncmp(name.c_str(), "//anon", sizeof("//anon")) == 0 ||
        EndsWith(name, ".ttf") || EndsWith(name, ".an") || EndsWith(name, ".ai")) {
        return false;
    }
    return true;
}

void DfxMaps::AddMap(std::shared_ptr<DfxMap> map, bool enableMapIndex)
{
    maps_.push_back(map);
    if (enableMapIndex) {
        mapIndex_.push_back(maps_.size() - 1);
    }
}

bool DfxMaps::FindMapByAddr(std::shared_ptr<DfxMap>& map, uintptr_t addr) const
{
    if ((maps_.size() == 0) || (addr == 0x0)) {
        return false;
    }

    size_t first = 0;
    size_t last = maps_.size();
    while (first < last) {
        size_t index = (first + last) / 2;
        const auto& cur = maps_[index];
        if (addr >= cur->begin && addr < cur->end) {
            map = cur;
            return true;
        } else if (addr < cur->begin) {
            last = index;
        } else {
            first = index + 1;
        }
    }
    return false;
}

bool DfxMaps::FindMapByFileInfo(std::shared_ptr<DfxMap>& map, std::string name, uint64_t offset) const
{
    for (auto &iter : maps_) {
        if (name != iter->name) {
            continue;
        }

        if (offset >= iter->offset && (offset - iter->offset) < (iter->end - iter->begin)) {
            LOGI("found name: %s, offset 0x%" PRIx64 " in map (%" PRIx64 "-%" PRIx64 " offset 0x%" PRIx64 ")",
                name.c_str(), offset, iter->begin, iter->end, iter->offset);
            map = iter;
            return true;
        }
    }
    return false;
}

bool DfxMaps::FindMapsByName(std::vector<std::shared_ptr<DfxMap>>& maps, std::string elfName) const
{
    if (maps_.empty()) {
        return false;
    }
    for (size_t i = 0; i < maps_.size(); ++i) {
        if (EndsWith(maps_[i]->name, elfName)) {
            maps.push_back(maps_[i]);
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
    if (!FindMapByAddr(map, addr)) {
        LOGU("Not mapped map for current addr.");
        return false;
    }

    if ((!EndsWith(map->name, "[anon:ArkTS Code]")) && (!EndsWith(map->name, "/dev/zero"))) {
        LOGU("Not ark map: %s", map->name.c_str());
        return false;
    }

    if ((map->prots & PROT_EXEC) == 0) {
        LOGU("current map is not executable.");
        return false;
    }

    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
