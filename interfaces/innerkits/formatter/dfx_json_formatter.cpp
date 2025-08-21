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
#include "json/json.h"
#endif

namespace OHOS {
namespace HiviewDFX {
#ifndef is_ohos_lite
namespace {
const int FRAME_BUF_LEN = 1024;
static std::string JsonAsString(const Json::Value& val)
{
    if (val.isConvertibleTo(Json::stringValue)) {
        return val.asString();
    }
    return "";
}

static bool FormatJsFrame(const Json::Value& frames, const uint32_t& frameIdx, std::string& outStr)
{
    const int jsIdxLen = 10;
    char buf[jsIdxLen] = { 0 };
    char idxFmt[] = "#%02u at";
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, idxFmt, frameIdx) <= 0) {
        return false;
    }
    outStr = std::string(buf);
    std::string symbol = JsonAsString(frames[frameIdx]["symbol"]);
    if (!symbol.empty()) {
        outStr.append(" " + symbol);
    }
    std::string packageName = JsonAsString(frames[frameIdx]["packageName"]);
    if (!packageName.empty()) {
        outStr.append(" " + packageName);
    }
    std::string file = JsonAsString(frames[frameIdx]["file"]);
    if (!file.empty()) {
        std::string line = JsonAsString(frames[frameIdx]["line"]);
        std::string column = JsonAsString(frames[frameIdx]["column"]);
        outStr.append(" (" + file + ":" + line + ":" + column + ")");
    }
    return true;
}

static bool FormatNativeFrame(const Json::Value& frames, const uint32_t& frameIdx, std::string& outStr)
{
    char buf[FRAME_BUF_LEN] = {0};
    char format[] = "#%02u pc %s %s";
    std::string buildId = JsonAsString(frames[frameIdx]["buildId"]);
    std::string file = JsonAsString(frames[frameIdx]["file"]);
    std::string offset = JsonAsString(frames[frameIdx]["offset"]);
    std::string pc = JsonAsString(frames[frameIdx]["pc"]);
    std::string symbol = JsonAsString(frames[frameIdx]["symbol"]);
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
    Json::Reader reader;
    Json::Value threads;
    if (!(reader.parse(jsonStack, threads))) {
        outStackStr.append("Failed to parse json stack info.");
        return false;
    }

    for (uint32_t i = 0; i < threads.size(); ++i) {
        std::string ss;
        Json::Value thread = threads[i];
        if (thread["tid"].isConvertibleTo(Json::stringValue) &&
            thread["thread_name"].isConvertibleTo(Json::stringValue)) {
            ss += "Tid:" + JsonAsString(thread["tid"]) + ", Name:" + JsonAsString(thread["thread_name"]) + "\n";
        }
        const Json::Value frames = thread["frames"];
        for (uint32_t j = 0; j < frames.size(); ++j) {
            std::string frameStr = "";
            bool formatStatus = false;
            if (JsonAsString(frames[j]["line"]).empty()) {
                formatStatus = FormatNativeFrame(frames, j, frameStr);
            } else {
                formatStatus = FormatJsFrame(frames, j, frameStr);
            }
            if (formatStatus) {
                ss += frameStr + "\n";
            } else {
                // Shall we try to print more information?
                outStackStr.append("Frame info is illegal.");
                return false;
            }
        }
        outStackStr.append(ss);
    }
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

static bool FormatKernelStackJson(std::vector<DfxThreadStack> processStack, std::string& formattedStack)
{
    if (processStack.empty()) {
        return false;
    }
    Json::Value jsonInfo;
    for (const auto &threadStack : processStack) {
        Json::Value threadInfo;
        threadInfo["thread_name"] = threadStack.threadName;
        threadInfo["tid"] = threadStack.tid;
        Json::Value frames(Json::arrayValue);
        for (const auto& frame : threadStack.frames) {
            Json::Value frameJson;
            char buf[FRAME_BUF_LEN] = {0};
            char format[] = "%016" PRIx64;
            if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, frame.relPc) <= 0) {
                continue;
            }
            frameJson["pc"] = std::string(buf);
            frameJson["symbol"] = "";
            frameJson["offset"] = 0;
            frameJson["file"] = frame.mapName.empty() ? "Unknown" : frame.mapName;
            frameJson["buildId"] = "";
            frames.append(frameJson);
        }
        threadInfo["frames"] = frames;
        jsonInfo.append(threadInfo);
    }
    formattedStack = Json::FastWriter().write(jsonInfo);
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
