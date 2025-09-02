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
#ifndef COREDUMP_CONFIG_MANGER_H
#define COREDUMP_CONFIG_MANGER_H

#include <string>
#include "cJSON.h"

namespace OHOS {
namespace HiviewDFX {
enum class DumpFormat {FULL, HWASAN, INVALID};

struct NoteDumpProcessConfig {
    bool prpsinfo {true};
    bool multiThread {true};
    bool auxv {true};
    bool dumpMappings {true};
};

struct NoteDumpThreadConfig {
    bool prstatus {true};
    bool fpregset {true};
    bool siginfo {true};
    bool dumpAll {true};
    bool armPacMask {true};
    bool armTaggedAddrCtrl {true};
};

struct LoadSegmentDumpConfig {
    uint64_t coredumpFilter {0};
    std::string excludePaths {""};
    std::string requiredPriority {"r"};
};

struct CoredumpConfig {
    bool coredumpSwitch {true};
    size_t maxCoredumpSize {0};
    int dumperThreadCount {1};
    std::string outputPath {"/data/storage/el2/base/files"};
    NoteDumpProcessConfig process;
    NoteDumpThreadConfig threads;
    LoadSegmentDumpConfig loadCfg;
};

class CoredumpConfigManager {
public:
    static CoredumpConfigManager& GetInstance();
    bool LoadCoredumpConfig(const std::string& profile);
    const CoredumpConfig& GetConfig() const;
private:
    void FillConfigFromObject(cJSON* obj);
    bool LoadProfileFromJson(const std::string& jsonText, const std::string& profileName);
    bool LoadProfileFromFile(const std::string& path, const std::string& profileName);
    void ParseProcessConfig(cJSON* obj);
    void ParseThreadConfig(cJSON* obj);
    void ParseGeneralFields(cJSON* obj);
    void ParseLoadConfig(cJSON* obj);
    CoredumpConfig dumpConfig_;
};

struct CJSONDeleter {
    void operator()(cJSON* ptr) const
    {
        if (ptr) {
            cJSON_Delete(ptr);
        }
    }
};

namespace CoredumpJsonUtil {
    int GetIntSafe(const cJSON* obj, const char* key, int defVal = 0);
    bool GetBoolSafe(const cJSON* obj, const char* key, bool defVal = false);
    const char* GetStrSafe(const cJSON* obj, const char* key, const char* defVal = "");
}
}
}
#endif
