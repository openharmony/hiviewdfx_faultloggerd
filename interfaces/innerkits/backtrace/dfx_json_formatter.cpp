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
#include <iostream>
#include <ostream>
#include <regex>
#include <securec.h>
#include <sstream>

#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_log.h"
#ifndef is_ohos_lite
#include "json/json.h"
#endif

namespace OHOS {
namespace HiviewDFX {
#ifndef is_ohos_lite
namespace {
const int FRAME_BUF_LEN = 1024;
static bool FormatJsFrame(const Json::Value& frames, const uint32_t& frameIdx, std::string& outStr)
{
    const int jsIdxLen = 10;
    char buf[jsIdxLen] = { 0 };
    char idxFmt[] = "#%02u at ";
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, idxFmt, frameIdx) <= 0) {
        return false;
    }
    outStr = std::string(buf, strlen(buf));
    std::string symbol = frames[frameIdx]["symbol"].asString();
    std::string file = frames[frameIdx]["file"].asString();
    std::string line = frames[frameIdx]["line"].asString();
    std::string column = frames[frameIdx]["column"].asString();
    outStr.append(symbol + " (" + file + ":" + line + ":" + column + ")");
    return true;
}

static bool FormatNativeFrame(const Json::Value& frames, const uint32_t& frameIdx, std::string& outStr)
{
    char buf[FRAME_BUF_LEN] = {0};
    char format[] = "#%02u pc %s %s";
    std::string buildId = frames[frameIdx]["buildId"].asString();
    std::string file = frames[frameIdx]["file"].asString();
    std::string offset = frames[frameIdx]["offset"].asString();
    std::string pc = frames[frameIdx]["pc"].asString();
    std::string symbol = frames[frameIdx]["symbol"].asString();
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, frameIdx, pc.c_str(),
                   file.empty() ? "Unknown" : file.c_str()) <= 0) {
        return false;
    }
    outStr = std::string(buf, strlen(buf));
    if (!symbol.empty()) {
        outStr.append("(" + symbol + "+" + offset + ")");
    }
    if (!buildId.empty()) {
        outStr.append("(" + buildId + ")");
    }
    return true;
}
}

bool DfxJsonFormatter::FormatJsonStack(std::string jsonStack, std::string& outStackStr)
{
    Json::Reader reader;
    Json::Value threads;
    if (!(reader.parse(jsonStack, threads))) {
        outStackStr.append("Failed to parse json stack info.");
        return false;
    }

    for (uint32_t i = 0; i < threads.size(); ++i) {
        std::ostringstream ss;
        Json::Value thread = threads[i];
        if (thread.isMember("tid") && thread.isMember("thread_name")) {
            ss << "Tid:" << thread["tid"].asString() << ", Name:" << thread["thread_name"].asString() << std::endl;
        }
        const Json::Value frames = thread["frames"];
        for (uint32_t j = 0; j < frames.size(); ++j) {
            std::string frameStr = "";
            bool formatStatus = false;
            if (frames[j]["line"].asString().empty()) {
                formatStatus = FormatNativeFrame(frames, j, frameStr);
            } else {
                formatStatus = FormatJsFrame(frames, j, frameStr);
            }
            if (formatStatus) {
                ss << frameStr << std::endl;
            } else {
                // Shall we try to print more information?
                outStackStr.append("Frame info is illegal.");
                return false;
            }
        }
        outStackStr.append(ss.str());
    }
    return true;
}

std::string DfxJsonFormatter::GetFramesJson(const std::vector<DfxFrame>& frames)
{
    char buf[FRAME_BUF_LEN] = {0};
#ifdef __LP64__
    char format[] = "%016" PRIx64 "";
#else
    char format[] = "%08" PRIx64 "";
#endif

    Json::Value framesJson;
    for (auto const& frame : frames) {
        Json::Value frameJson;
        if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, frame.relPc) > 0) {
            frameJson["pc"] = buf;
        } else {
            frameJson["pc"] = frame.relPc;
        }
        frameJson["symbol"] = frame.funcName;
        frameJson["offset"] = frame.funcOffset;
        frameJson["file"] = frame.mapName;
        frameJson["buildId"] = frame.buildId;
        framesJson.append(frameJson);
    }
    return Json::FastWriter().write(framesJson);
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
