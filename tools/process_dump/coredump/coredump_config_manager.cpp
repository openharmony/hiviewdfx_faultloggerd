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
#include "coredump_config_manager.h"

#include "dfx_log.h"

#include <string>
#include <fstream>
#include "file_util.h"

namespace OHOS {
namespace HiviewDFX {

using namespace CoredumpJsonUtil;

namespace {
    constexpr const char* const COREDUMP_CONFIG_PATH = "/system/etc/fault_coredump.json";
};

CoredumpConfigManager& CoredumpConfigManager::GetInstance()
{
    static CoredumpConfigManager instance;
    return instance;
}

const CoredumpConfig& CoredumpConfigManager::GetConfig() const
{
    return dumpConfig_;
}

void CoredumpConfigManager::ParseProcessConfig(cJSON* obj)
{
    cJSON* procCfg = cJSON_GetObjectItem(obj, "process");
    if (!cJSON_IsObject(procCfg)) {
        return;
    }

    dumpConfig_.process.prpsinfo = GetIntSafe(procCfg, "prpsinfo");
    dumpConfig_.process.multiThread = GetBoolSafe(procCfg, "multiThread");
    dumpConfig_.process.auxv = GetIntSafe(procCfg, "auxv");
    dumpConfig_.process.dumpMappings = GetBoolSafe(procCfg, "dumpMappings");
}

void CoredumpConfigManager::ParseThreadConfig(cJSON* obj)
{
    cJSON* threadCfg = cJSON_GetObjectItem(obj, "threads");
    if (!cJSON_IsObject(threadCfg)) {
        return;
    }

    dumpConfig_.threads.prstatus = GetIntSafe(threadCfg, "prstatus");
    dumpConfig_.threads.fpregset = GetIntSafe(threadCfg, "fpregset");
    dumpConfig_.threads.siginfo = GetIntSafe(threadCfg, "siginfo");
    dumpConfig_.threads.dumpAll = GetBoolSafe(threadCfg, "dumpAll");
    dumpConfig_.threads.armPacMask = GetIntSafe(threadCfg, "armPacMask");
    dumpConfig_.threads.armTaggedAddrCtrl = GetIntSafe(threadCfg, "armTaggedAddrCtrl");
}

void CoredumpConfigManager::ParseGeneralFields(cJSON* obj)
{
    dumpConfig_.coredumpSwitch = GetBoolSafe(obj, "coredumpSwitch");
    dumpConfig_.dumperThreadCount = GetIntSafe(obj, "dumperThreadCount", 1);
}

void CoredumpConfigManager::ParseLoadConfig(cJSON* obj)
{
    cJSON* loadCfg = cJSON_GetObjectItem(obj, "loadCfg");
    if (!cJSON_IsObject(loadCfg)) {
        return;
    }

    dumpConfig_.loadCfg.excludePaths = GetStrSafe(loadCfg, "excludePaths");
    dumpConfig_.loadCfg.requiredPriority = GetStrSafe(loadCfg, "requiredPriority");
}

void CoredumpConfigManager::FillConfigFromObject(cJSON* obj)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return;
    }

    ParseProcessConfig(obj);
    ParseThreadConfig(obj);
    ParseGeneralFields(obj);
    ParseLoadConfig(obj);
}

bool CoredumpConfigManager::LoadProfileFromJson(const std::string& jsonText, const std::string& profileName)
{
    std::unique_ptr<cJSON, CJSONDeleter> root(cJSON_Parse(jsonText.c_str()));

    if (!root) {
        DFXLOGE("JSON parse error: %{public}s", cJSON_GetErrorPtr());
        return false;
    }

    cJSON* profiles = cJSON_GetObjectItem(root.get(), "coredumpProfiles");
    if (!profiles) {
        DFXLOGE("Missing 'coredumpProfiles' object");
        return false;
    }

    cJSON* profile = cJSON_GetObjectItem(profiles, profileName.c_str());
    if (!profile) {
        DFXLOGE("Profile %{public}s not found", profileName.c_str());
        return false;
    }

    FillConfigFromObject(profile);
    return true;
}

bool CoredumpConfigManager::LoadProfileFromFile(const std::string& path, const std::string& profileName)
{
    std::string content;
    LoadStringFromFile(path, content);
    if (content.empty()) {
        DFXLOGE("Cannot content open config file: %{public}s, errno %{public}d", path.c_str(), errno);
        return false;
    }
    return LoadProfileFromJson(content, profileName);
}

bool CoredumpConfigManager::LoadCoredumpConfig(const std::string& profile)
{
    DFXLOGI("config path %{private}s", COREDUMP_CONFIG_PATH);
    if (!LoadProfileFromFile(COREDUMP_CONFIG_PATH, profile)) {
        return false;
    }
    return true;
}

int CoredumpJsonUtil::GetIntSafe(const cJSON* obj, const char* key, int defVal)
{
    if (obj == nullptr || key == nullptr) {
        return defVal;
    }
    cJSON* item = cJSON_GetObjectItem(obj, key);
    return cJSON_IsNumber(item) ? item->valueint : defVal;
}

bool CoredumpJsonUtil::GetBoolSafe(const cJSON* obj, const char* key, bool defVal)
{
    if (obj == nullptr || key == nullptr) {
        return defVal;
    }
    cJSON* item = cJSON_GetObjectItem(obj, key);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : defVal;
}

const char* CoredumpJsonUtil::GetStrSafe(const cJSON* obj, const char* key, const char* defVal)
{
    if (obj == nullptr || key == nullptr) {
        return defVal;
    }
    cJSON* item = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : defVal;
}
}
}
