/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "dfx_map.h"

#include <algorithm>
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
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMap"
}

std::shared_ptr<DfxMap> DfxMap::Create(const std::string buf, size_t size)
{
    auto map = std::make_shared<DfxMap>();
    if (map->Parse(buf, size) == false) {
        return nullptr;
    }
    return map;
}

bool DfxMap::Parse(const std::string buf, size_t size)
{
#if defined(is_ohos) && is_ohos
    if (buf.empty() || size == 0) {
        return false;
    }
    uint32_t pos = 0;
    char permChs[5] = {0}; // 5:rwxp

    // 7658d38000-7658d40000 rw-p 00000000 00:00 0                              [anon:thread signal stack]
    if (sscanf_s(buf.c_str(), "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %x:%x %" SCNxPTR " %n",
        &begin, &end, &permChs, sizeof(permChs), &offset, &major, &minor, &inode, &pos) != 7) { // 7:scan size
        LOGW("Failed to parse map info: %s.", buf.c_str());
        return false;
    }
    this->perms = std::string(permChs, sizeof(permChs));
    PermsToProts(this->perms, this->prots, this->flag);
    TrimAndDupStr(buf.substr(pos), this->name);
    return true;
#else
    return false;
#endif
}

bool DfxMap::IsMapExec()
{
    if ((prots & PROT_EXEC) != 0) {
        return true;
    }
    return false;
}

bool DfxMap::IsArkExecutable()
{
    if (name.length() == 0) {
        return false;
    }

    if ((!StartsWith(name, "[anon:ArkTS Code")) && (!StartsWith(name, "/dev/zero")) && (!EndsWith(name, "stub.an"))) {
        return false;
    }

    if (!IsMapExec()) {
        LOGU("Current ark map(%s) is not exec", name.c_str());
        return false;
    }
    LOGU("Current ark map: %s", name.c_str());
    return true;
}

uint64_t DfxMap::GetRelPc(uint64_t pc)
{
    if (GetElf() != nullptr) {
        return GetElf()->GetRelPc(pc, begin, offset);
    }
    return (pc - begin + offset);
}

std::string DfxMap::ToString()
{
    char buf[LINE_BUF_SIZE] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%" PRIx64 "-%" PRIx64 " %s %08" PRIx64 " %s\n", \
        begin, end, perms.c_str(), offset, name.c_str());
    if (ret <= 0) {
        LOGE("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    return std::string(buf);
}

void DfxMap::PermsToProts(const std::string perms, uint32_t& prots, uint32_t& flag)
{
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

    if (perms.find("p") != std::string::npos) {
        flag = MAP_PRIVATE;
    } else if (perms.find("s") != std::string::npos) {
        flag = MAP_SHARED;
    }
}

const std::shared_ptr<DfxElf> DfxMap::GetElf(pid_t pid)
{
    if (elf == nullptr) {
        if (name.empty()) {
            LOGE("%s", "Invalid map, name empty.");
            return nullptr;
        }
        LOGU("GetElf name: %s", name.c_str());
        if (EndsWith(name, ".hap")) {
            elf = DfxElf::CreateFromHap(name, prevMap, offset);
        } else if (name == "shmm" && IsMapExec()) {
#if is_ohos && !is_mingw
            size_t size = end - begin;
            shmmData = std::make_shared<std::vector<uint8_t>>(size);
            size_t byte = DfxMemory::ReadProcMemByPid(pid, begin, shmmData->data(), size);
            if (byte != size) {
                LOGE("%s", "Failed to read shmm data");
                return nullptr;
            }
            elf = std::make_shared<DfxElf>(shmmData->data(), byte);
#endif
        } else {
            elf = DfxElf::Create(name);
        }
    }
    return elf;
}

std::string DfxMap::GetElfName()
{
    if (name.empty() || GetElf() == nullptr) {
        return name;
    }
    std::string soName = name;
    if (EndsWith(name, ".hap")) {
        soName.append("!" + elf->GetElfName());
    }
    return soName;
}
} // namespace HiviewDFX
} // namespace OHOS
