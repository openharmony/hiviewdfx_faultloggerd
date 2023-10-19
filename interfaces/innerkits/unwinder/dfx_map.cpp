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

#include "dfx_map.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMap"
}

std::shared_ptr<DfxMap> DfxMap::Create(const std::string buf, int size)
{
    auto map = std::make_shared<DfxMap>();
    if (map->Parse(buf, size) == false) {
        return nullptr;
    }
    return map;
}

bool DfxMap::Parse(const std::string buf, int size)
{
#if is_ohos
    int pos = 0;
    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    uint64_t major = 0;
    uint64_t minor = 0;
    ino_t inode = 0;
    char perms[5] = {0}; // 5:rwxp

    // 7658d38000-7658d40000 rw-p 00000000 00:00 0                              [anon:thread signal stack]
    if (sscanf_s(buf.c_str(), "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %x:%x %" SCNxPTR " %n",
        &begin, &end, &perms, sizeof(perms), &offset, &major, &minor, &inode, &pos) != 7) { // 7:scan size
        DFXLOG_WARN("Fail to parse maps info.");
        return false;
    }

    this->begin = begin;
    this->end = end;
    this->offset = offset;
    this->major = major;
    this->minor = minor;
    this->inode = inode;
    this->perms = std::string(perms, sizeof(perms));
    PermsToProts(this->perms, this->prots, this->flag);
    TrimAndDupStr(buf.substr(pos), this->name);
    if (!IsValidName()) {
        this->prots |= PROT_DEVICE_MAP;
    }
    return true;
#else
    return false;
#endif
}

bool DfxMap::IsValidName()
{
    if (name.length() == 0) {
        return false;
    }

    if (strncmp(name.c_str(), "/dev/", 5) == 0) { // 5:length of "/dev/"
        return false;
    }

    if ((prots & PROT_EXEC) == 0) {
        return false;
    }
    return true;
}

bool DfxMap::IsArkName()
{
    if (name.length() == 0) {
        return false;
    }

    if ((strstr(name.c_str(), "[anon:ArkTS Code]") == nullptr) &&
        (strstr(name.c_str(), "/dev/zero") == nullptr)) {
        return false;
    }
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
        DFXLOG_ERROR("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
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

const std::shared_ptr<DfxElf>& DfxMap::GetElf()
{
    if (elf == nullptr) {
        if (!name.empty()) {
            LOGU("GetElf name: %s", name.c_str());
            elf = DfxElf::Create(name);
        }
    }
    return elf;
}
} // namespace HiviewDFX
} // namespace OHOS
