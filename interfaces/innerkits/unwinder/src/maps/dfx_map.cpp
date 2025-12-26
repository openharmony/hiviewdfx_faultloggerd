/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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
#if is_mingw
#include "dfx_nonlinux_define.h"
#else
#include <sys/mman.h>
#endif

#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_hap.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "elf_factory_selector.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMap"

const char* const MAP_ARKWEB_CORE_PREFIX = "/data/storage/el1/bundle/arkwebcore/";
const char* const ARKWEB_CORE_REAL_PATH = "/data/app/el1/bundle/public/com.huawei.hmos.arkwebcore/";
const char* const SANDBOX_PATH_PREFIX = "/data/storage/el1/bundle/";
const char* const BUNDLE_PATH_PREFIX = "/data/app/el1/bundle/public/";

#if defined(is_ohos) && is_ohos

AT_ALWAYS_INLINE bool SkipWhiteSpace(const char* buff, const size_t buffSize, size_t& buffOffset)
{
    if (buff == nullptr) {
        return false;
    }
    while (buffOffset < buffSize) {
        const char item = buff[buffOffset];
        if (item != ' ' && item != '\t') {
            return true;
        }
        buffOffset++;
    }
    return false;
}

AT_ALWAYS_INLINE bool ScanHex(const char* buff, const size_t buffSize, uint64_t& valp, size_t& buffOffset)
{
    if (!SkipWhiteSpace(buff, buffSize, buffOffset)) {
        return false;
    }
    valp = 0;
    unsigned cnt = 0;
    while (buffOffset < buffSize) {
        unsigned char digit = buff[buffOffset];
        if (digit >= '0' && digit <= '9') {
            digit -= '0';
        } else if (digit >= 'a' && digit <= 'f') {
            digit -= 'a' - 10; // 10 : base 10
        } else if (digit >= 'A' && digit <= 'F') {
            digit -= 'A' - 10; // 10 : base 10
        } else {
            break;
        }
        constexpr uint64_t hex = 4;
        valp = (valp << hex) | digit;
        ++cnt;
        buffOffset++;
    }
    return cnt > 0;
}

AT_ALWAYS_INLINE bool ScanDec(const char* buff, const size_t buffSize, uint64_t& valp, size_t& buffOffset)
{
    if (!SkipWhiteSpace(buff, buffSize, buffOffset)) {
        return false;
    }
    valp = 0;
    unsigned int cnt = 0;
    while (buffOffset < buffSize) {
        unsigned char digit = buff[buffOffset];
        if (digit >= '0' && digit <= '9') {
            digit -= '0';
        } else {
            break;
        }
        constexpr uint64_t dec = 10;
        valp = (valp * dec) + digit;
        ++cnt;
        buffOffset++;
    }
    return cnt > 0;
}

AT_ALWAYS_INLINE bool ScanChar(const char* buff, const size_t buffSize, char& valp, size_t& buffOffset)
{
    if (!SkipWhiteSpace(buff, buffSize, buffOffset)) {
        return false;
    }
    valp = buff[buffOffset];
    if (valp) {
        buffOffset++;
    }
    return true;
}

AT_ALWAYS_INLINE bool ScanString(const char* buff, const size_t buffSize, std::string& valp, size_t& buffOffset)
{
    if (!SkipWhiteSpace(buff, buffSize, buffOffset)) {
        return false;
    }
    const auto beginOffSet = buffOffset;
    while (buffOffset < buffSize) {
        const char cp = buff[buffOffset];
        if (cp == ' ' || cp == '\t' || cp == '\0') {
            break;
        }
        buffOffset++;
    }
    if (beginOffSet < buffOffset) {
        valp = std::string(buff + beginOffSet, buffOffset - beginOffSet);
        return true;
    }
    return false;
}

AT_ALWAYS_INLINE bool PermsToProtsAndFlag(const char* permChs, const size_t sz, uint32_t& prots, uint32_t& flag)
{
    if (permChs == nullptr || sz < 4) { // 4 : min perms size
        return false;
    }

    size_t i = 0;
    if (permChs[i] == 'r') {
        prots |= PROT_READ;
    } else if (permChs[i] != '-') {
        return false;
    }
    i++;

    if (permChs[i] == 'w') {
        prots |= PROT_WRITE;
    } else if (permChs[i] != '-') {
        return false;
    }
    i++;

    if (permChs[i] == 'x') {
        prots |= PROT_EXEC;
    } else if (permChs[i] != '-') {
        return false;
    }
    i++;

    if (permChs[i] == 'p') {
        flag = MAP_PRIVATE;
    } else if (permChs[i] == 's') {
        flag = MAP_SHARED;
    } else {
        return false;
    }

    return true;
}
#endif
}

std::shared_ptr<DfxMap> DfxMap::Create(const std::string& vma)
{
    if (vma.empty()) {
        return nullptr;
    }
    auto map = std::make_shared<DfxMap>();
    if (!map->Parse(vma.c_str(), vma.size())) {
        DFXLOGW("Failed to parse map: %{public}s", vma.c_str());
        return nullptr;
    }
    return map;
}

bool DfxMap::Parse(const char* buff, size_t buffSize)
{
#if defined(is_ohos) && is_ohos
    constexpr size_t maxDfxMapSize = 4 * 1024;
    if (buffSize >= maxDfxMapSize) {
        return false;
    }
    // 7658d38000-7658d40000 rw-p 00000000 00:00 0                              [anon:thread signal stack]
    /* scan: "begin-end perms offset major:minor inum path" */
    size_t buffOffset = 0;
    ScanHex(buff, buffSize, begin, buffOffset);
    char dashChar = 0;
    ScanChar(buff, buffSize, dashChar, buffOffset);
    if (dashChar != '-') {
        return false;
    }
    ScanHex(buff, buffSize, end, buffOffset);
    ScanString(buff, buffSize, perms, buffOffset);
    if (!PermsToProtsAndFlag(perms.c_str(), perms.size(), prots, flag)) {
        return false;
    }
    ScanHex(buff, buffSize, offset, buffOffset);
    ScanHex(buff, buffSize, major, buffOffset);
    ScanChar(buff, buffSize, dashChar, buffOffset);
    if (dashChar != ':') {
        return false;
    }
    ScanHex(buff, buffSize, minor, buffOffset);
    uint64_t inodeValue = 0;
    ScanDec(buff, buffSize, inodeValue, buffOffset);
    inode = static_cast<ino_t>(inodeValue);
    if (buffOffset < buffSize) {
        TrimAndDupStr(buff + buffOffset, buffSize - buffOffset, name);
    }
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
        DFXLOGU("Current ark map(%{public}s) is not exec", name.c_str());
        return false;
    }
    DFXLOGU("Current ark map: %{public}s", name.c_str());
    return true;
}

bool DfxMap::IsJsvmExecutable()
{
    if (name.empty()) {
        return false;
    }
    if (StartsWith(name, "[anon:JSVM_JIT")) {
        return true;
    }
    if (!EndsWith(name, "libv8_shared.so")) {
        return false;
    }

    if (!IsMapExec()) {
        DFXLOGU("Current jsvm map(%{public}s) is not exec", name.c_str());
        return false;
    }
    DFXLOGU("Current jsvm map: %{public}s", name.c_str());
    return true;
}

bool DfxMap::IsVdsoMap()
{
    if ((name == "[shmm]" || name == "[vdso]") && IsMapExec()) {
        return true;
    }
    return false;
}

uint64_t DfxMap::GetRelPc(uint64_t pc)
{
    if (GetElf() != nullptr) {
        return GetElf()->GetRelPc(pc, begin, offset);
    }
    if (prevMap != nullptr) {
        return (pc - prevMap->begin);
    }
    return (pc - begin + offset);
}

std::string DfxMap::ToString() const
{
    char buf[LINE_BUF_SIZE] = {0};
    std::string realMapName = UnFormatMapName(name);
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%" PRIx64 "-%" PRIx64 " %s %08" PRIx64 " %s\n", \
        begin, end, perms.c_str(), offset, realMapName.c_str());
    if (ret <= 0) {
        DFXLOGE("%{public}s :: snprintf_s failed, line: %{public}d.", __func__, __LINE__);
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

const std::shared_ptr<DfxHap> DfxMap::GetHap()
{
    if (hap == nullptr) {
        hap = std::make_shared<DfxHap>();
    }
    return hap;
}

const std::shared_ptr<DfxElf> DfxMap::GetElf(pid_t pid)
{
    if (elf == nullptr) {
        if (name.empty()) {
            DFXLOGE("Invalid map, name empty.");
            return nullptr;
        }
        auto elfFactory = ElfFactorySelector::Select(*this, pid);
        if (elfFactory != nullptr) {
            elf = elfFactory->Create();
        }
        if (elf != nullptr && !elf->IsValid()) {
            elf = nullptr;
        }
        // adlt need update lodeBase
        if (elf != nullptr && elf->IsAdlt()) {
            elf->SetLoadBase(GetAdltLoadBase());
        }
        DFXLOGU("GetElf name: %{public}s", name.c_str());
    }
    return elf;
}

const std::shared_ptr<DfxElf> DfxMap::GetElfLongLong(long long pid)
{
    return GetElf(static_cast<pid_t>(pid));
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

void DfxMap::FormatMapName(const std::string& bundleName, std::string& mapName, bool& isArkWeb)
{
    if (mapName.empty()) {
        return;
    }
    // arkwebcore process get real path
    if (StartsWith(mapName, MAP_ARKWEB_CORE_PREFIX)) {
        mapName = ARKWEB_CORE_REAL_PATH + mapName.substr(strlen(MAP_ARKWEB_CORE_PREFIX));
        if (isArkWeb) {
            return;
        }
        isArkWeb = EndsWith(mapName, "libarkweb_engine.so");
        return;
    }
    if (StartsWith(mapName, SANDBOX_PATH_PREFIX)) {
        mapName = BUNDLE_PATH_PREFIX + bundleName + "/" + mapName.substr(strlen(SANDBOX_PATH_PREFIX));
    }
}

void DfxMap::FormatMapName(pid_t pid, std::string& mapName)
{
    if (pid <= 0 || pid == getpid()) {
        return;
    }

    // format sandbox file path, add '/proc/xxx/root' prefix
    if (StartsWith(mapName, "/data/storage/")) {
        mapName = "/proc/" + std::to_string(pid) + "/root" + mapName;
    }
}

std::string DfxMap::UnFormatMapName(const std::string& mapName)
{
    // unformat sandbox file path, drop '/proc/xxx/root' prefix
    if (StartsWith(mapName, "/proc/")) {
        auto startPos = mapName.find("/data/storage/");
        if (startPos != std::string::npos) {
            return mapName.substr(startPos);
        }
    }
    return mapName;
}
} // namespace HiviewDFX
} // namespace OHOS
