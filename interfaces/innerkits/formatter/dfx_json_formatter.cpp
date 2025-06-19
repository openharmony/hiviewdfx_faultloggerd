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

#include "dfx_json_formatter.h"

#include <cstdlib>
#include <securec.h>
#include "dfx_kernel_stack.h"
#ifndef is_ohos_lite
#include "cJSON.h"
#endif

namespace OHOS {
namespace HiviewDFX {
#ifndef is_ohos_lite
namespace {
const int FRAME_BUF_LEN = 1024;
static bool IsConvertToString(const cJSON *json)
{
    if (json == nullptr) {
        return false;
    }
    return cJSON_IsString(json) || cJSON_IsNumber(json) || cJSON_IsBool(json) || cJSON_IsNull(json);
}

static std::string GetStringValueFromItem(const cJSON *item)
{
    std::string ret{};
    if (cJSON_IsString(item)) {
        ret = item->valuestring;
    } else if (cJSON_IsNumber(item)) {
        ret = std::to_string(item->valueint);
    } else if (cJSON_IsBool(item)) {
        ret = cJSON_IsTrue(item) ? "true" : "false";
    } else {
        ret = "";
    }
    return ret;
}

static std::string GetStringValue(const cJSON *json, const std::string& key)
{
    if (!cJSON_IsObject(json)) {
        return "";
    }
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key.c_str());
    return GetStringValueFromItem(item);
}

static bool FormatJsFrame(const cJSON *frames, const uint32_t& frameIdx, std::string& outStr)
{
    const int jsIdxLen = 10;
    char buf[jsIdxLen] = { 0 };
    char idxFmt[] = "#%02u at";
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, idxFmt, frameIdx) <= 0) {
        return false;
    }
    outStr = std::string(buf);
    cJSON *item = cJSON_GetArrayItem(frames, frameIdx);
    if (item == nullptr) {
        return false;
    }
    std::string symbol = GetStringValue(item, "symbol");
    if (!symbol.empty()) {
        outStr.append(" " + symbol);
    }
    std::string packageName = GetStringValue(item, "packageName");
    if (!packageName.empty()) {
        outStr.append(" " + packageName);
    }
    std::string file = GetStringValue(item, "file");
    if (!file.empty()) {
        std::string line = GetStringValue(item, "line");
        std::string column = GetStringValue(item, "column");
        outStr.append(" (" + file + ":" + line + ":" + column + ")");
    }
    return true;
}

static bool FormatNativeFrame(const cJSON *frames, const uint32_t& frameIdx, std::string& outStr)
{
    char buf[FRAME_BUF_LEN] = {0};
    char format[] = "#%02u pc %s %s";
    cJSON *item = cJSON_GetArrayItem(frames, frameIdx);
    if (item == nullptr) {
        return false;
    }
    std::string buildId = GetStringValue(item, "buildId");
    std::string file = GetStringValue(item, "file");
    std::string offset = GetStringValue(item, "offset");
    std::string pc = GetStringValue(item, "pc");
    std::string symbol = GetStringValue(item, "symbol");
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, frameIdx, pc.c_str(),
                   file.empty() ? "Unknown" : file.c_str()) <= 0) {
        return false;
    }
    outStr = std::string(buf);
    if (!symbol.empty()) {
        outStr.append("(" + symbol + "+" + offset + ")");
    }
    if (!buildId.empty()) {
        outStr.append("(" + buildId + ")");
    }
    return true;
}
}

bool DfxJsonFormatter::FormatJsonStack(const std::string& jsonStack, std::string& outStackStr)
{
    cJSON *root = cJSON_Parse(jsonStack.c_str());
    if (root == nullptr) {
        outStackStr.append("Failed to parse json stack info.");
        return false;
    }

    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        std::string ss;
        cJSON *thread_tid = cJSON_GetObjectItemCaseSensitive(item, "tid");
        cJSON *thread_name = cJSON_GetObjectItemCaseSensitive(item, "thread_name");
        if ((IsConvertToString(thread_tid)) && (IsConvertToString(thread_name))) {
            ss += "Tid:" + GetStringValueFromItem(thread_tid) +
                ", Name:" + GetStringValueFromItem(thread_name) + "\n";
        }
        const cJSON *frames = cJSON_GetObjectItemCaseSensitive(item, "frames");
        int frameSize = cJSON_GetArraySize(frames);
        for (int j = 0; j < frameSize; ++j) {
            std::string frameStr = "";
            bool formatStatus = false;
            std::string line = GetStringValue(cJSON_GetArrayItem(frames, j), "line");
            if (line.empty()) {
                formatStatus = FormatNativeFrame(frames, j, frameStr);
            } else {
                formatStatus = FormatJsFrame(frames, j, frameStr);
            }
            if (formatStatus) {
                ss += frameStr + "\n";
            } else {
                // Shall we try to print more information?
                outStackStr.append("Frame info is illegal.");
                cJSON_Delete(root);
                return false;
            }
        }
        outStackStr.append(ss);
    }
    cJSON_Delete(root);
    return true;
}

#ifdef __aarch64__
static bool FormatKernelStackStr(const std::vector<DfxThreadStack>& processStack, std::string& formattedStack)
{
    if (processStack.empty()) {
        return false;
    }
    formattedStack = "";
    for (const auto &threadStack : processStack) {
        std::string ss = "Tid:" + std::to_string(threadStack.tid) + ", Name:" + threadStack.threadName + "\n";
        formattedStack.append(ss);
        for (size_t frameIdx = 0; frameIdx < threadStack.frames.size(); ++frameIdx) {
            std::string file = threadStack.frames[frameIdx].mapName;
            char buf[FRAME_BUF_LEN] = {0};
            char format[] = "#%02zu pc %016" PRIx64 " %s";
            if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, frameIdx, threadStack.frames[frameIdx].relPc,
                file.empty() ? "Unknown" : file.c_str()) <= 0) {
                continue;
            }
            formattedStack.append(std::string(buf, strlen(buf)) + "\n");
        }
    }
    return true;
}

static void AddItemToKernelStack(const DfxThreadStack& threadStack, cJSON *jsonInfo)
{
    cJSON *threadInfo = cJSON_CreateObject();
    if (threadInfo == nullptr) {
        return;
    }
    cJSON_AddStringToObject(threadInfo, "thread_name", threadStack.threadName.c_str());
    cJSON_AddNumberToObject(threadInfo, "tid", threadStack.tid);
    cJSON *frames = cJSON_CreateArray();
    if (frames == nullptr) {
        cJSON_Delete(threadInfo);
        return;
    }
    for (const auto& frame : threadStack.frames) {
        cJSON *frameJson = cJSON_CreateObject();
        if (frameJson == nullptr) {
            continue;
        }
        char buf[FRAME_BUF_LEN] = {0};
        char format[] = "%016" PRIx64;
        if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, frame.relPc) <= 0) {
            cJSON_Delete(frameJson);
            continue;
        }
        cJSON_AddStringToObject(frameJson, "pc", buf);
        cJSON_AddStringToObject(frameJson, "symbol", "");
        cJSON_AddNumberToObject(frameJson, "offset", 0);
        std::string file = frame.mapName.empty() ? "Unknown" : frame.mapName;
        cJSON_AddStringToObject(frameJson, "file", file.c_str());
        cJSON_AddStringToObject(frameJson, "buildId", "");
        cJSON_AddItemToArray(frames, frameJson);
    }
    cJSON_AddItemToObject(threadInfo, "frames", frames);
    cJSON_AddItemToArray(jsonInfo, threadInfo);
}

static bool FormatKernelStackJson(std::vector<DfxThreadStack> processStack, std::string& formattedStack)
{
    if (processStack.empty()) {
        return false;
    }
    cJSON *jsonInfo = cJSON_CreateArray();
    if (jsonInfo == nullptr) {
        return false;
    }
    for (const auto &threadStack : processStack) {
        AddItemToKernelStack(threadStack, jsonInfo);
    }
    auto itemStr = cJSON_PrintUnformatted(jsonInfo);
    formattedStack = itemStr;
    cJSON_free(itemStr);
    cJSON_Delete(jsonInfo);
    return true;
}
#endif

bool DfxJsonFormatter::FormatKernelStack(const std::string& kernelStack, std::string& formattedStack, bool jsonFormat)
{
#ifdef __aarch64__
    std::vector<DfxThreadStack> processStack;
    if (!FormatProcessKernelStack(kernelStack, processStack)) {
        return false;
    }
    if (jsonFormat) {
        return FormatKernelStackJson(processStack, formattedStack);
    } else {
        return FormatKernelStackStr(processStack, formattedStack);
    }
#else
    return false;
#endif
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
