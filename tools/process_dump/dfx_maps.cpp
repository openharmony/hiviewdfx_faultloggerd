/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains process dump dfx maps module. */

#include "dfx_maps.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <securec.h>

#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_log.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
static const int MAPINFO_SIZE = 256;

std::shared_ptr<DfxElfMaps> DfxElfMaps::Create(pid_t pid)
{
    DfxLogDebug("Enter %s.", __func__);
    auto dfxElfMaps = std::make_shared<DfxElfMaps>();

    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/maps", pid) <= 0) {
        DfxLogWarn("Fail to print path.");
        return nullptr;
    }

    char realPath[PATH_MAX] = {0};
    if (realpath(path, realPath) == NULL) {
        DfxLogWarn("Maps path(%s) is not exist.", path);
        return nullptr;
    }

    FILE *fp = fopen(realPath, "r");
    if (fp == nullptr) {
        DfxLogWarn("Fail to open maps info.");
        return nullptr;
    }

    char mapInfo[MAPINFO_SIZE] = {0};
    while (fgets(mapInfo, sizeof(mapInfo), fp) != nullptr) {
        std::shared_ptr<DfxElfMap> map = DfxElfMap::Create(mapInfo, sizeof(mapInfo));
        if (!map) {
            DfxLogWarn("Fail to init map info:%s.", mapInfo);
            continue;
        } else {
            dfxElfMaps->InsertMapToElfMaps(map);
        }
    }
    int ret = fclose(fp);
    if (ret < 0) {
        DfxLogWarn("Fail to close maps info.");
        return nullptr;
    }
    DfxLogDebug("Exit %s.", __func__);
    return dfxElfMaps;
}

std::shared_ptr<DfxElfMap> DfxElfMap::Create(const std::string mapInfo, int size)
{
    DfxLogDebug("Enter %s.", __func__);
    auto dfxElfMap = std::make_shared<DfxElfMap>();

    int pos = 0;
    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    char perms[5] = {0}; // 5:rwxp
    std::string path = "";

    // 7658d38000-7658d40000 rw-p 00000000 00:00 0                              [anon:thread signal stack]
    if (sscanf_s(mapInfo.c_str(), "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*x:%*x %*d%n", &begin, &end,
        &perms, sizeof(perms), &offset,
        &pos) != 4) { // 4:scan size
        DfxLogWarn("Fail to parse maps info.");
        return nullptr;
    }

    dfxElfMap->SetMapBegin(begin);
    dfxElfMap->SetMapEnd(end);
    dfxElfMap->SetMapOffset(offset);
    dfxElfMap->SetMapPerms(perms, sizeof(perms));
    TrimAndDupStr(mapInfo.substr(pos), path);
    dfxElfMap->SetMapPath(path);
    DfxLogDebug("Exit %s.", __func__);
    return dfxElfMap;
}

void DfxElfMaps::InsertMapToElfMaps(std::shared_ptr<DfxElfMap> map)
{
    DfxLogDebug("Enter %s.", __func__);
    maps_.push_back(map);

    return;
}


bool DfxElfMaps::FindMapByPath(const std::string path, std::shared_ptr<DfxElfMap>& map) const
{
    DfxLogDebug("Enter %s.", __func__);
    for (auto iter = maps_.begin(); iter != maps_.end(); iter++) {
        if ((*iter)->GetMapPath() == "") {
            continue;
        }

        if (strcmp(path.c_str(), (*iter)->GetMapPath().c_str()) == 0) {
            map = *iter;
            return true;
        }
    }
    DfxLogDebug("Exit %s.", __func__);
    return false;
}

bool DfxElfMaps::FindMapByAddr(uintptr_t address, std::shared_ptr<DfxElfMap>& map) const
{
    DfxLogDebug("Enter %s.", __func__);
    for (auto iter = maps_.begin(); iter != maps_.end(); iter++) {
        if (((*iter)->GetMapBegin() < address) && ((*iter)->GetMapEnd() > address)) {
            map = *iter;
            return true;
        }
    }
    DfxLogDebug("Exit %s.", __func__);
    return false;
}

bool DfxElfMaps::CheckPcIsValid(uint64_t pc) const
{
    DfxLogDebug("Enter %s.", __func__);

    bool ret = false;

    do {
        if (pc == 0x0) {
            break;
        }

        std::shared_ptr<DfxElfMap> map;
        for (auto iter = maps_.begin(); iter != maps_.end(); iter++) {
            if (((*iter)->GetMapBegin() < pc) && ((*iter)->GetMapEnd() > pc)) {
                map = *iter;
                std::string perms = map->GetMapPerms();
                if (perms.find("x") != std::string::npos) {
                    ret = true;
                    break;
                }
            }
        }
    } while (false);
    
    DfxLogDebug("Exit %s :: ret(%d).", __func__, ret);
    return ret;
}

bool DfxElfMap::IsVaild()
{
    DfxLogDebug("Enter %s.", __func__);
    if (path_.length() == 0) {
        return false;
    }

    if (strncmp(path_.c_str(), "/dev/", 5) == 0) { // 5:length of "/dev/"
        return false;
    }

    if (strncmp(path_.c_str(), "[anon:", 6) == 0) { // 6:length of "[anon:"
        return false;
    }

    if (strncmp(path_.c_str(), "/system/framework/", 18) == 0) { // 18:length of "/system/framework/"
        return false;
    }
    DfxLogDebug("Exit %s.", __func__);
    return true;
}

void DfxElfMap::PrintMap(int32_t fd)
{
    WriteLog(fd, "%" PRIx64 "-%" PRIx64 " %s %08" PRIx64 " %s\n",
        begin_, end_, perms_.c_str(), offset_, path_.c_str());
}

uint64_t DfxElfMap::GetMapBegin() const
{
    return begin_;
}

uint64_t DfxElfMap::GetMapEnd() const
{
    return end_;
}

uint64_t DfxElfMap::GetMapOffset() const
{
    return offset_;
}

std::string DfxElfMap::GetMapPerms() const
{
    return perms_;
}

std::string DfxElfMap::GetMapPath() const
{
    return path_;
}

std::shared_ptr<DfxElf> DfxElfMap::GetMapImage() const
{
    return image_;
}

void DfxElfMap::SetMapBegin(uint64_t begin)
{
    begin_ = begin;
}

void DfxElfMap::SetMapEnd(uint64_t end)
{
    end_ = end;
}

void DfxElfMap::SetMapOffset(uint64_t offset)
{
    offset_ = offset;
}

void DfxElfMap::SetMapPerms(const std::string perms, int size)
{
    perms_ = perms;
}

void DfxElfMap::SetMapPath(const std::string path)
{
    path_ = path;
}

void DfxElfMap::SetMapImage(std::shared_ptr<DfxElf> image)
{
    image_ = image;
}

std::vector<std::shared_ptr<DfxElfMap>> DfxElfMaps::GetValues() const
{
    return maps_;
}
} // namespace HiviewDFX
} // namespace OHOS
