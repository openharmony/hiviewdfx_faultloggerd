/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "coredump_mapping_manager.h"

#include <fstream>
#include <elf.h>
#include <numeric>

#include "dfx_log.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr int HEX_BASE = 16;
}
CoredumpMappingManager& CoredumpMappingManager::GetInstance()
{
    static CoredumpMappingManager instance;
    return instance;
}

void CoredumpMappingManager::Parse(pid_t pid)
{
    if (pid <= 0) {
        return;
    }
    pid_ = pid;
    std::string path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream file(path);
    if (!file.is_open()) {
        DFXLOGE("open %{public}s fail, errno %{public}d", path.c_str(), errno);
        return;
    }
    std::string line;
    DumpMemoryRegions region;
    while (getline(file, line)) {
        ObtainDumpRegion(line, region);
        maps_.push_back(region);
    }
}

bool CoredumpMappingManager::ShouldIncludeRegion(const DumpMemoryRegions& region)
{
    std::string perms = region.priority;
    if (getuid() == 0) {
        if (strstr(region.pathName, "jemalloc") != nullptr) {
            return perms.find('p') != std::string::npos && perms.find('r') != std::string::npos;
        }
        return false;
    }
    return perms.find('p') != std::string::npos && perms.find('r') != std::string::npos;
}

bool CoredumpMappingManager::IsHwAsanProcess()
{
    return std::any_of(maps_.begin(), maps_.end(), [](auto const& region) {
        std::string pathName = region.pathName;
        return pathName.find("libclang_rt.hwasan.so") != std::string::npos;
    });
}

uint64_t CoredumpMappingManager::EstimateFileSize()
{
    auto noteCount = static_cast<uint64_t>(
        std::count_if(maps_.begin(), maps_.end(), [](const DumpMemoryRegions& region) {return true;}));

    uint64_t headerSize = sizeof(Elf64_Ehdr) + (noteCount + 1) * sizeof(Elf64_Phdr) +
        (noteCount + 2) * sizeof(Elf64_Shdr);

    uint64_t notesSize = noteCount * (sizeof(Elf64_Nhdr) + 100); // 100 : reserve NT_FILE space

    uint64_t totalMem = std::accumulate(maps_.begin(), maps_.end(), uint64_t{0}, [](uint64_t sum, auto const &r) {
        return ShouldIncludeRegion(r) ? sum + r.memorySizeHex : sum;
    });

    return headerSize + notesSize + totalMem + 1000; // 1000 : reserve space
}

void CoredumpMappingManager::ObtainDumpRegion(std::string &line, DumpMemoryRegions &region)
{
    auto ret = sscanf_s(line.c_str(), "%[^-\n]-%[^ ] %s %s %s %u %[^\n]",
        region.start, sizeof(region.start),
        region.end, sizeof(region.end),
        region.priority, sizeof(region.priority),
        region.offset, sizeof(region.offset),
        region.dev, sizeof(region.dev),
        &(region.inode),
        region.pathName, sizeof(region.pathName));
    if (ret != 7) { // 7 : params need all success
        region.pathName[0] = '\0';
    }

    region.startHex = strtoul(region.start, nullptr, HEX_BASE);
    region.endHex = strtoul(region.end, nullptr, HEX_BASE);
    region.offsetHex = strtoul(region.offset, nullptr, HEX_BASE);
    region.memorySizeHex = region.endHex - region.startHex;
}
}
}
