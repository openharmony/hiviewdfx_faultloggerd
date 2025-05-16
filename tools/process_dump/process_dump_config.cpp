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

#include "process_dump_config.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <securec.h>
#include <string>
#ifndef is_ohos_lite
#include "cJSON.h"
#endif
#include "dfx_define.h"
#include "dfx_log.h"
#include "file_ex.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "ProcessDumpConfig"

const char* const KEY_THREAD_DUMP_INFO = "KeyThreadDumpInfo";
const char* const DUMP_INFO_HEADER = "DumpInfoHeader";
const char* const SUBMITTER_STACK = "SubmitterStack";
const char* const REGISTERS = "Registers";
const char* const EXTRA_CRASH_INFO = "ExtraCrashInfo";
const char* const OTHER_THREAD_DUMP_INFO = "OtherThreadDumpInfo";
const char* const MEMORY_NEAR_REGISTER = "MemoryNearRegister";
const char* const FAULT_STACK = "FaultStack";
const char* const MAPS = "Maps";
const char* const OPEN_FILES = "OpenFiles";
#ifndef is_ohos_lite
constexpr int32_t MAX_NUM_INT32 = std::numeric_limits<int32_t>::max();
const char* const DUMP_INFO_COMPONENT_NAME[] = {
    KEY_THREAD_DUMP_INFO, DUMP_INFO_HEADER, SUBMITTER_STACK, REGISTERS, EXTRA_CRASH_INFO,
    OTHER_THREAD_DUMP_INFO, MEMORY_NEAR_REGISTER, FAULT_STACK, MAPS, OPEN_FILES,
};
const char* const PROCESS_DUMP_CONF_PATH = "/system/etc/faultlogger.conf";
std::string GetStringValueFromJson(const cJSON* json, std::string defaultValue = "")
{
    if (!cJSON_IsString(json)) {
        return defaultValue;
    }
    return json->valuestring;
}

int32_t GetInt32ValueFromJson(const cJSON* json, int32_t defaultValue = 0)
{
    if (!cJSON_IsNumber(json)) {
        return defaultValue;
    }
    return json->valueint;
}

bool GetBoolValueFromJson(const cJSON* json, bool defaultValue = false)
{
    if (!cJSON_IsBool(json)) {
        return defaultValue;
    }
    return cJSON_IsTrue(json) == 1;
}

void ParseDumpInfoConfig(const cJSON* json, DumpConfig& dumpConfig)
{
    auto* dumpInfos = cJSON_GetObjectItem(json, "DumpInfos");
    if (dumpInfos == nullptr) {
        DFXLOGE("Failed to Parse Dump Info Config!");
        return;
    }
    if (!cJSON_IsArray(dumpInfos)) {
        DFXLOGE("dumpInfos is not array!");
        return;
    }
    int size = cJSON_GetArraySize(dumpInfos);
    for (int i = 0; i < size; i++) {
        auto* dumpInfo = cJSON_GetArrayItem(dumpInfos, i);
        int32_t code = GetInt32ValueFromJson(cJSON_GetObjectItem(dumpInfo, "Code"));
        std::string typeName = GetStringValueFromJson(cJSON_GetObjectItem(dumpInfo, "TypeName"));
        std::vector<std::string> dumpInfoComponent;
        for (const auto& componentName : DUMP_INFO_COMPONENT_NAME) {
            if (GetBoolValueFromJson(cJSON_GetObjectItem(dumpInfo, componentName))) {
                dumpInfoComponent.emplace_back(componentName);
            }
        }
        dumpConfig.dumpInfo[static_cast<ProcessDumpType>(code)] = dumpInfoComponent;
    }
}

void ParseConfig(const cJSON* json, DumpConfig& dumpConfig)
{
    if (json == nullptr) {
        DFXLOGE("failed to Parse directoryConfig");
        return;
    }
    dumpConfig.maxFrameNums = std::clamp(
        GetInt32ValueFromJson(cJSON_GetObjectItem(json, "maxFrameNums")), 0, MAX_NUM_INT32);
    dumpConfig.faultStackLowAddrStep = std::clamp(
        GetInt32ValueFromJson(cJSON_GetObjectItem(json, "faultStackLowAddrStep")), 0, MAX_NUM_INT32);
    dumpConfig.faultStackHighAddrStep = std::clamp(
        GetInt32ValueFromJson(cJSON_GetObjectItem(json, "faultStackHighAddrStep")), 0, MAX_NUM_INT32);
    dumpConfig.extendPcLrPrinting = GetBoolValueFromJson(cJSON_GetObjectItem(json, "extendPcLrPrinting"));
    dumpConfig.simplifyVmaPrinting = GetBoolValueFromJson(cJSON_GetObjectItem(json, "simplifyVmaPrinting"));
    dumpConfig.reservedParseSymbolTime = std::clamp(
        GetInt32ValueFromJson(cJSON_GetObjectItem(json, "reservedParseSymbolTime")), 0, MAX_NUM_INT32);
    ParseDumpInfoConfig(json, dumpConfig);
}
#endif
}

ProcessDumpConfig& ProcessDumpConfig::GetInstance()
{
    static ProcessDumpConfig processDumpConfig;
    return processDumpConfig;
}

#ifndef is_ohos_lite
ProcessDumpConfig::ProcessDumpConfig()
{
    std::string content;
    OHOS::LoadStringFromFile(PROCESS_DUMP_CONF_PATH, content);
    cJSON* json = cJSON_Parse(content.c_str());
    if (json == nullptr) {
        DFXLOGE("failed to parse json from the content of file(%{public}s).", PROCESS_DUMP_CONF_PATH);
        LoadDefaultConfig();
        return;
    }
    ParseConfig(json, dumpConfig_);
    cJSON_Delete(json);
}
#else
ProcessDumpConfig::ProcessDumpConfig()
{
    LoadDefaultConfig();
}
#endif

void ProcessDumpConfig::LoadDefaultConfig()
{
    dumpConfig_.dumpInfo[ProcessDumpType::DUMP_TYPE_CPP_CRASH] = {
        KEY_THREAD_DUMP_INFO, DUMP_INFO_HEADER, SUBMITTER_STACK,
        REGISTERS, EXTRA_CRASH_INFO, OTHER_THREAD_DUMP_INFO,
        MEMORY_NEAR_REGISTER, FAULT_STACK, MAPS, OPEN_FILES,
    };
    dumpConfig_.dumpInfo[ProcessDumpType::DUMP_TYPE_DUMP_CATCH] = {
        KEY_THREAD_DUMP_INFO, DUMP_INFO_HEADER, OTHER_THREAD_DUMP_INFO,
    };
    dumpConfig_.dumpInfo[ProcessDumpType::DUMP_TYPE_MEM_LEAK] = {
        KEY_THREAD_DUMP_INFO, DUMP_INFO_HEADER, REGISTERS,
        OTHER_THREAD_DUMP_INFO, MEMORY_NEAR_REGISTER, FAULT_STACK, MAPS,
    };
    dumpConfig_.dumpInfo[ProcessDumpType::DUMP_TYPE_FDSAN] = {
        KEY_THREAD_DUMP_INFO, DUMP_INFO_HEADER, REGISTERS,
        MEMORY_NEAR_REGISTER, FAULT_STACK, MAPS, OPEN_FILES,
    };
    dumpConfig_.dumpInfo[ProcessDumpType::DUMP_TYPE_JEMALLOC] = {
        KEY_THREAD_DUMP_INFO, DUMP_INFO_HEADER, REGISTERS,
        MEMORY_NEAR_REGISTER, FAULT_STACK, MAPS,
    };
    dumpConfig_.dumpInfo[ProcessDumpType::DUMP_TYPE_BADFD] = {
        KEY_THREAD_DUMP_INFO, DUMP_INFO_HEADER, REGISTERS,
        MEMORY_NEAR_REGISTER, FAULT_STACK, MAPS, OPEN_FILES,
    };
}

const DumpConfig& ProcessDumpConfig::GetConfig() const
{
    return dumpConfig_;
}
} // namespace HiviewDFX
} // namespace OHOS
