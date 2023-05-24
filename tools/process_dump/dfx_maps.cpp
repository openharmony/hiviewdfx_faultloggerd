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

/* This files contains process dump dfx maps module. */
#include "dfx_maps.h"

#include <algorithm>
#include <cinttypes>
#include <climits>
#include <cstdio>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <securec.h>
#include <string>
#include <vector>
#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_log.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
static const int MAPINFO_SIZE = 256;
MAYBE_UNUSED static constexpr int X86_ONE_STEP_NORMAL = 1;
MAYBE_UNUSED static constexpr int ARM_TWO_STEP_NORMAL = 2;
MAYBE_UNUSED static constexpr int ARM_FOUR_STEP_NORMAL = 4;

std::shared_ptr<DfxElfMaps> DfxElfMaps::Create(pid_t pid)
{
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/maps", pid) <= 0) {
        DFXLOG_WARN("Fail to print path.");
        return nullptr;
    }

    return Create(path);
}

std::shared_ptr<DfxElfMaps> DfxElfMaps::CreateFromLocal()
{
    return Create(std::string("/proc/self/maps"));
}

std::shared_ptr<DfxElfMaps> DfxElfMaps::Create(const std::string path)
{
    char realPath[PATH_MAX] = {0};
    if (realpath(path.c_str(), realPath) == nullptr) {
        DFXLOG_WARN("Maps path(%s) is not exist.", path.c_str());
        return nullptr;
    }

    FILE *fp = fopen(realPath, "r");
    if (fp == nullptr) {
        DFXLOG_WARN("Fail to open maps info.");
        return nullptr;
    }

    auto dfxElfMaps = std::make_shared<DfxElfMaps>();
    char mapInfo[MAPINFO_SIZE] = {0};
    while (fgets(mapInfo, sizeof(mapInfo), fp) != nullptr) {
        std::shared_ptr<DfxElfMap> map = DfxElfMap::Create(mapInfo, sizeof(mapInfo));
        if (!map) {
            DFXLOG_WARN("Fail to init map info:%s.", mapInfo);
            continue;
        } else {
            dfxElfMaps->InsertMapToElfMaps(map);
        }
    }
    int ret = fclose(fp);
    if (ret < 0) {
        DFXLOG_WARN("Fail to close maps info.");
    }
    dfxElfMaps->Sort();
    return dfxElfMaps;
}

std::shared_ptr<DfxElfMap> DfxElfMap::Create(const std::string mapInfo, int size)
{
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
        DFXLOG_WARN("Fail to parse maps info.");
        return nullptr;
    }

    dfxElfMap->SetMapBegin(begin);
    dfxElfMap->SetMapEnd(end);
    dfxElfMap->SetMapOffset(offset);
    dfxElfMap->SetMapPerms(perms, sizeof(perms));
    TrimAndDupStr(mapInfo.substr(pos), path);
    dfxElfMap->SetMapPath(path);
    return dfxElfMap;
}

void DfxElfMaps::InsertMapToElfMaps(std::shared_ptr<DfxElfMap> map)
{
    maps_.push_back(map);
    return;
}


bool DfxElfMaps::FindMapByPath(const std::string path, std::vector<std::shared_ptr<DfxElfMap>>& maps) const
{
    bool ret = false;
    for (auto iter = maps_.begin(); iter != maps_.end(); iter++) {
        if ((*iter)->GetMapPath() == "") {
            continue;
        }

        if (strcmp(path.c_str(), (*iter)->GetMapPath().c_str()) == 0) {
            maps.push_back(*iter);
            ret = true;
        }
    }
    return ret;
}

bool DfxElfMaps::FindMapByAddr(uintptr_t address, std::shared_ptr<DfxElfMap>& map) const
{
    if ((maps_.size() == 0) || (address == 0x0)) {
        return false;
    }

    size_t first = 0;
    size_t last = maps_.size();
    while (first < last) {
        size_t index = (first + last) / 2;
        const auto& cur = maps_[index];
        if (address >= cur->GetMapBegin() && address < cur->GetMapEnd()) {
            map = cur;
            return true;
        } else if (address < cur->GetMapBegin()) {
            last = index;
        } else {
            first = index + 1;
        }
    }
    return false;
}

void DfxElfMaps::Sort(void)
{
    std::sort(maps_.begin(), maps_.end(),
        [](const std::shared_ptr<DfxElfMap>& a, const std::shared_ptr<DfxElfMap>& b) {
        return a->GetMapBegin() < b->GetMapBegin();
    });
}

bool DfxElfMaps::CheckPcIsValid(uint64_t pc) const
{
    DFXLOG_DEBUG("%s :: pc(0x%x).", __func__, pc);

    std::shared_ptr<DfxElfMap> map = nullptr;
    if (FindMapByAddr(pc, map) == false) {
        return false;
    }

    if (map != nullptr) {
        std::string perms = map->GetMapPerms();
        if (perms.find("x") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool DfxElfMap::IsValid()
{
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
    return true;
}

std::string DfxElfMap::PrintMap()
{
    char buf[LOG_BUF_LEN] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%" PRIx64 "-%" PRIx64 " %s %08" PRIx64 " %s\n", \
        begin_, end_, perms_.c_str(), offset_, path_.c_str());
    if (ret <= 0) {
        DFXLOG_ERROR("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    return std::string(buf);
}

uint64_t DfxElfMap::GetRelPc(uint64_t pc)
{
    uint64_t relPc = 0;
    if (GetMapImage() == nullptr) {
        SetMapImage(DfxElf::Create(GetMapPath().c_str()));
    }

    if (GetMapImage() == nullptr) {
        relPc = pc - (GetMapBegin() - GetMapOffset());
    } else {
        relPc = (pc - GetMapBegin()) + GetMapImage()->FindRealLoadOffset(GetMapOffset());
    }

#ifdef __aarch64__
    relPc = relPc - 4; // 4 : instr offset
#elif defined(__x86_64__)
    relPc = relPc - 1; // 1 : instr offset
#endif
    return relPc;
}

uint32_t DfxElfMap::GetPcAdjustment(uint64_t pc)
{
    if (pc <= ARM_FOUR_STEP_NORMAL) { // pc zero is abnormal case, so we don't adjust pc.
        return 0;
    }
    uint32_t sz = ARM_FOUR_STEP_NORMAL;
#if defined(__arm__)
    if (pc & 1) {
        // This is a thumb instruction, it could be 2 or 4 bytes.
        uint32_t value;
        uint64_t adjustedPc = pc - 5;
        if ((GetMapPerms().find("r") == std::string::npos) ||
            adjustedPc < GetMapBegin() ||
            (adjustedPc + sizeof(value)) >= GetMapEnd() ||
            memcpy_s(&value, sizeof(value), reinterpret_cast<const void *>(adjustedPc), sizeof(value)) ||
            (value & 0xe000f000) != 0xe000f000) {
            sz = ARM_TWO_STEP_NORMAL;
        }
    }
#elif defined(__aarch64__)
    sz = ARM_FOUR_STEP_NORMAL;
#elif defined(__x86_64__)
    sz = X86_ONE_STEP_NORMAL;
#endif
    return sz;
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
    return elf_;
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

void DfxElfMap::SetMapImage(std::shared_ptr<DfxElf> elf)
{
    elf_ = elf;
}

std::vector<std::shared_ptr<DfxElfMap>> DfxElfMaps::GetValues() const
{
    return maps_;
}
} // namespace HiviewDFX
} // namespace OHOS
