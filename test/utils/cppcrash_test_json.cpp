/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "cppcrash_test_json.h"
#include "file_util.h"
#include <iostream>

namespace OHOS {
namespace HiviewDFX {

CppCrashTestJson::CppCrashTestJson() : root_(nullptr) {}

CppCrashTestJson::~CppCrashTestJson()
{
    Clear();
}

void CppCrashTestJson::Clear()
{
    if (root_ != nullptr) {
        cJSON_Delete(root_);
        root_ = nullptr;
    }
}

void CppCrashTestJson::InitFromFile(const std::string& filePath)
{
    std::string content;
    if (!LoadStringFromFile(filePath, content)) {
        std::cout << "Failed to load file: " << filePath << std::endl;
        return;
    }
    cJSON* json = cJSON_Parse(content.c_str());
    if (json == nullptr) {
        std::cout << "Failed to parse json from file: " << filePath << std::endl;
        return;
    }
    Clear();
    root_ = json;
    json = nullptr;
}

std::string CppCrashTestJson::ToTextString()
{
    if (root_ == nullptr) {
        return "";
    }
    std::string result;
    result += AppendHeaderFields();
    result += AppendProcessFields();
    result += AppendKeyThreadInfo();
    result += AppendStackTraceFields();
    result += AppendOtherThreadInfo();
    result += AppendMemoryFields();
    return result;
}

std::string CppCrashTestJson::AppendConfigInfo()
{
    std::string result;
    cJSON* item = cJSON_GetObjectItem(root_, "ENABLED_APP_LOG_CONFIG");
    if (cJSON_IsObject(item)) {
        std::string configInfo;
        cJSON* subItem = nullptr;
        subItem = cJSON_GetObjectItem(item, "EXTEND_PC_LR_PRINTING");
        if (cJSON_IsTrue(subItem)) {
            configInfo += "Extend pc lr printing:true\n";
        }
        subItem = cJSON_GetObjectItem(item, "LOG_CUT_OFF_SIZE");
        if (cJSON_IsString(subItem) && subItem->valuestring) {
            configInfo += "Log cut off size:" + std::string(subItem->valuestring);
        }
        subItem = cJSON_GetObjectItem(item, "SIMPLIFY_MAPS_PRINTING");
        if (cJSON_IsTrue(subItem)) {
            configInfo += "Simplify maps printing:true\n";
        }
        subItem = cJSON_GetObjectItem(item, "MERGE_APP_LOG_PRINTING");
        if (cJSON_IsTrue(subItem)) {
            configInfo += "Merge app log printing:true\n";
        }
        subItem = cJSON_GetObjectItem(item, "ENABLE_MINIDUMP_LOG");
        if (cJSON_IsTrue(subItem)) {
            configInfo += "Enable minidump log:true\n";
        }
        if (!configInfo.empty()) {
            result += "Enabled app log configs:\n" + configInfo;
        }
    }
    return result;
}

std::string CppCrashTestJson::AppendHeaderFields()
{
    std::string result;
    cJSON* item = cJSON_GetObjectItem(root_, "BUILD_INFO");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Build info:" + std::string(item->valuestring) + "\n";
    }
    result += AppendConfigInfo();
    item = cJSON_GetObjectItem(root_, "TIMESTAMP");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Timestamp:" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "PID");
    if (cJSON_IsNumber(item)) {
        result += StringPrintf("Pid:%d\n", item->valueint);
    }
    item = cJSON_GetObjectItem(root_, "UID");
    if (cJSON_IsNumber(item)) {
        result += StringPrintf("Uid:%d\n", item->valueint);
    }
    item = cJSON_GetObjectItem(root_, "HITRACEID");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "HiTraceId:" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "PNAME");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Process name:" + std::string(item->valuestring) + "\n";
    }
    item = cJSON_GetObjectItem(root_, "VSNPRINTF_ERROR");
    if (cJSON_IsString(item) && item->valuestring) {
        result += std::string(item->valuestring);
    }
    return result;
}

std::string CppCrashTestJson::AppendProcessFields()
{
    std::string result;
    cJSON* item = cJSON_GetObjectItem(root_, "PROCESS_LIFETIME");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Process life time:" + std::string(item->valuestring) + "\n";
    }
    item = cJSON_GetObjectItem(root_, "PROCESS_RSS_MEMINFO");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Process Memory(kB):" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "REASON");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Reason:" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "LAST_FATAL_MESSAGE");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "LastFatalMessage:" + std::string(item->valuestring) + "\n";
    }
    return result;
}

std::string CppCrashTestJson::AppendStackTraceFields()
{
    std::string result;
    cJSON* item = cJSON_GetObjectItem(root_, "SUBMITTER_STACKTRACE");
    if (cJSON_IsString(item) && item->valuestring) {
        result += std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "KEY_THREAD_REGISTERS");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Registers:\n" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "EXTRA_CRASH_INFO");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Extra crash info:\n" + std::string(item->valuestring);
    }
    return result;
}

std::string CppCrashTestJson::AppendOtherThreadInfo()
{
    std::string result;
    cJSON* item = cJSON_GetObjectItem(root_, "OTHER_THREAD_INFO");
    if (cJSON_IsArray(item)) {
        result += "Other thread info:\n";
        int size = cJSON_GetArraySize(item);
        for (int i = 0; i < size; i++) {
            cJSON* threadItem = cJSON_GetArrayItem(item, i);
            if (threadItem != nullptr) {
                result += ThreadJsonToText(threadItem);
            }
        }
    }
    return result;
}

std::string CppCrashTestJson::AppendMemoryFields()
{
    std::string result;
    cJSON* item = cJSON_GetObjectItem(root_, "MEMORY_NEAR_REGISTERS");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Memory near registers:\n" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "FAULT_STACK");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "FaultStack:\n" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "PROCESS_MAPS");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "Maps:\n" + std::string(item->valuestring);
    }
    item = cJSON_GetObjectItem(root_, "OPEN_FILES");
    if (cJSON_IsString(item) && item->valuestring) {
        result += "OpenFiles:\n" + std::string(item->valuestring);
    }
    return result;
}

std::string CppCrashTestJson::AppendKeyThreadInfo()
{
    std::string result;
    cJSON* item = cJSON_GetObjectItem(root_, "KEY_THREAD_INFO");
    if (item != nullptr) {
        result += "Fault thread info:\n";
        result += ThreadJsonToText(item);
    }
    return result;
}

std::string CppCrashTestJson::ThreadJsonToText(cJSON* threadJson)
{
    if (threadJson == nullptr) {
        return "";
    }
    std::string result;
    cJSON* threadName = cJSON_GetObjectItem(threadJson, "thread_name");
    cJSON* tid = cJSON_GetObjectItem(threadJson, "tid");
    if (cJSON_IsString(threadName) && threadName->valuestring && cJSON_IsNumber(tid)) {
        result += StringPrintf("Tid:%d, Name:%s\n", tid->valueint, threadName->valuestring);
    }
    cJSON* frames = cJSON_GetObjectItem(threadJson, "frames");
    if (cJSON_IsArray(frames)) {
        result += FramesJsonToText(frames);
    }
    return result;
}

std::string CppCrashTestJson::FramesJsonToText(cJSON* framesJson)
{
    if (!cJSON_IsArray(framesJson)) {
        return "";
    }
    std::string result;
    int size = cJSON_GetArraySize(framesJson);
    for (int i = 0; i < size; i++) {
        cJSON* frame = cJSON_GetArrayItem(framesJson, i);
        if (frame != nullptr) {
            result += FrameJsonToText(frame, i);
        }
    }
    return result;
}

std::string CppCrashTestJson::FrameJsonToText(cJSON* frameJson, int index)
{
    if (frameJson == nullptr) {
        return "";
    }
    if (cJSON_HasObjectItem(frameJson, "packageName")) {
        return JsFrameJsonToText(frameJson, index);
    }
    return NativeFrameJsonToText(frameJson, index);
}

std::string CppCrashTestJson::JsFrameJsonToText(cJSON* frameJson, int index)
{
    std::string result;
    result += StringPrintf("#%02d at", index);

    cJSON* symbol = cJSON_GetObjectItem(frameJson, "symbol");
    if (cJSON_IsString(symbol) && symbol->valuestring && strlen(symbol->valuestring) > 0) {
        result += " " + std::string(symbol->valuestring);
    }

    cJSON* packageName = cJSON_GetObjectItem(frameJson, "packageName");
    if (cJSON_IsString(packageName) && packageName->valuestring) {
        result += " " + std::string(packageName->valuestring);
    }

    cJSON* file = cJSON_GetObjectItem(frameJson, "file");
    cJSON* line = cJSON_GetObjectItem(frameJson, "line");
    cJSON* column = cJSON_GetObjectItem(frameJson, "column");
    if (cJSON_IsString(file) && file->valuestring && strlen(file->valuestring) > 0) {
        result += " (" + std::string(file->valuestring);
        if (cJSON_IsNumber(line)) {
            result += ":" + std::to_string(line->valueint);
        }
        if (cJSON_IsNumber(column)) {
            result += ":" + std::to_string(column->valueint);
        }
        result += ")";
    }
    result += "\n";
    return result;
}

std::string CppCrashTestJson::NativeFrameJsonToText(cJSON* frameJson, int index)
{
    std::string result;
    cJSON* pc = cJSON_GetObjectItem(frameJson, "pc");
    cJSON* file = cJSON_GetObjectItem(frameJson, "file");
    cJSON* symbol = cJSON_GetObjectItem(frameJson, "symbol");
    cJSON* offset = cJSON_GetObjectItem(frameJson, "offset");
    cJSON* buildId = cJSON_GetObjectItem(frameJson, "buildId");

    result += StringPrintf("#%02d pc ", index);
    if (cJSON_IsString(pc) && pc->valuestring) {
        result += std::string(pc->valuestring) + " ";
    } else {
        result += "00000000 ";
    }
    if (cJSON_IsString(file) && file->valuestring && strlen(file->valuestring) > 0) {
        result += std::string(file->valuestring);
    } else {
        result += " [Unknown]";
    }
    if (cJSON_IsString(symbol) && symbol->valuestring && strlen(symbol->valuestring) > 0) {
        result += " (" + std::string(symbol->valuestring);
        if (cJSON_IsNumber(offset)) {
            result += StringPrintf("+%d", static_cast<int>(offset->valuedouble));
        }
        result += ")";
    }
    if (cJSON_IsString(buildId) && buildId->valuestring && strlen(buildId->valuestring) > 0) {
        result += " (" + std::string(buildId->valuestring) + ")";
    }
    result += "\n";
    return result;
}

} // namespace HiviewDFX
} // namespace OHOS