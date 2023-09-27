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
#include <cinttypes>
#include <climits>
#include <cstdio>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <securec.h>
#include <sys/mman.h>
#include <string>
#include <sstream>
#include <vector>
#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_memory_file.h"
#include "dfx_log.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
static const int MAPINFO_SIZE = 256;
MAYBE_UNUSED static constexpr int X86_ONE_STEP_NORMAL = 1;
MAYBE_UNUSED static constexpr int ARM_TWO_STEP_NORMAL = 2;
MAYBE_UNUSED static constexpr int ARM_FOUR_STEP_NORMAL = 4;
MAYBE_UNUSED static constexpr int PROT_DEVICE_MAP = 0x8000;

std::shared_ptr<DfxElfMap> DfxElfMap::Create(const std::string mapBuf, int size)
{
    auto elfMap = std::make_shared<DfxElfMap>();

    int pos = 0;
    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    char perms[5] = {0}; // 5:rwxp

    // 7658d38000-7658d40000 rw-p 00000000 00:00 0                              [anon:thread signal stack]
    if (sscanf_s(mapBuf.c_str(), "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*x:%*x %*d%n", &begin, &end,
        &perms, sizeof(perms), &offset,
        &pos) != 4) { // 4:scan size
        DFXLOG_WARN("Fail to parse maps info.");
        return nullptr;
    }

    elfMap->begin = begin;
    elfMap->end = end;
    elfMap->offset = offset;
    elfMap->perms = std::string(perms, sizeof(perms));
    PermsToProts(elfMap->perms, elfMap->prots);
    TrimAndDupStr(mapBuf.substr(pos), elfMap->path);
    if (!elfMap->IsValidPath()) {
        elfMap->prots |= PROT_DEVICE_MAP;
    }
    return elfMap;
}

std::shared_ptr<DfxElfMaps> DfxElfMaps::Create(pid_t pid)
{
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/maps", pid) <= 0) {
        DFXLOG_WARN("Fail to print path.");
        return nullptr;
    }
    return Create(std::string(path));
}

std::shared_ptr<DfxElfMaps> DfxElfMaps::CreateFromLocal()
{
    return Create(std::string(PROC_SELF_MAPS_PATH));
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
    char mapBuf[MAPINFO_SIZE] = {0};
    while (fgets(mapBuf, sizeof(mapBuf), fp) != nullptr) {
        std::shared_ptr<DfxElfMap> map = DfxElfMap::Create(mapBuf, sizeof(mapBuf));
        if (map == nullptr) {
            DFXLOG_WARN("Fail to init map info:%s.", mapBuf);
            continue;
        } else {
            dfxElfMaps->AddMap(map);
        }
    }
    int ret = fclose(fp);
    if (ret < 0) {
        DFXLOG_WARN("Fail to close maps info.");
    }
    dfxElfMaps->Sort();
    return dfxElfMaps;
}

std::shared_ptr<DfxElfMaps> DfxElfMaps::Create(const char* buffer)
{
    if (buffer == nullptr) {
        return nullptr;
    }
    std::istringstream iss(buffer);
    std::string temp;
    auto dfxElfMaps = std::make_shared<DfxElfMaps>();
    while (std::getline(iss, temp)) {
        std::shared_ptr<DfxElfMap> map = DfxElfMap::Create(temp, temp.length());
        if (map == nullptr) {
            DFXLOG_WARN("Fail to init map info:%s.", temp.c_str());
            continue;
        } else {
            dfxElfMaps->AddMap(map);
        }
    }
    dfxElfMaps->Sort();
    return dfxElfMaps;
}

void DfxElfMaps::AddMap(std::shared_ptr<DfxElfMap> map)
{
    maps_.push_back(map);
    return;
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
        if (address >= cur->begin && address < cur->end) {
            map = cur;
            return true;
        } else if (address < cur->begin) {
            last = index;
        } else {
            first = index + 1;
        }
    }
    return false;
}

void DfxElfMaps::Sort(bool less)
{
    if (less) {
        std::sort(maps_.begin(), maps_.end(),
            [](const std::shared_ptr<DfxElfMap>& a, const std::shared_ptr<DfxElfMap>& b) {
            return a->begin < b->begin;
        });
    } else {
        std::sort(maps_.begin(), maps_.end(),
            [](const std::shared_ptr<DfxElfMap>& a, const std::shared_ptr<DfxElfMap>& b) {
            return a->begin > b->begin;
        });
    }
}

const std::vector<std::shared_ptr<DfxElfMap>>& DfxElfMaps::GetMaps() const
{
    return maps_;
}

bool DfxElfMap::IsValidPath()
{
    if (path.length() == 0) {
        return false;
    }

    if (strncmp(path.c_str(), "/dev/", 5) == 0) { // 5:length of "/dev/"
        return false;
    }
    return true;
}

std::string DfxElfMap::PrintMap()
{
    char buf[LINE_BUF_SIZE] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%" PRIx64 "-%" PRIx64 " %s %08" PRIx64 " %s\n", \
        begin, end, perms.c_str(), offset, path.c_str());
    if (ret <= 0) {
        DFXLOG_ERROR("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    return std::string(buf);
}

void DfxElfMap::PermsToProts(const std::string perms, uint64_t& prots)
{
    prots = PROT_NONE;
    // rwxp
    if (perms.find("r") != std::string::npos) {
        prots |= PROT_READ;
    }

    if (perms.find("w") != std::string::npos) {
        prots |= PROT_WRITE;
    }

    if (perms.find("x") != std::string::npos) {
        prots |= PROT_EXEC;
    }
}

uint64_t DfxElfMap::GetRelPc(uint64_t pc)
{
    uint64_t relPc = 0;
    if (elf == nullptr) {
        auto memory = DfxMemoryFile::CreateFileMemory(path, 0);
        if (memory != nullptr) {
            elf = std::make_shared<DfxElf>(memory);
        }
    }

    if (elf == nullptr) {
        relPc = pc - (begin - offset);
    } else {
        relPc = (pc - begin) + elf->GetRealLoadOffset(offset);
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
        if ((perms.find("r") == std::string::npos) ||
            adjustedPc < begin || (adjustedPc + sizeof(value)) >= end ||
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
} // namespace HiviewDFX
} // namespace OHOS
