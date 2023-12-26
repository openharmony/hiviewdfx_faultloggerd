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

#include "dfx_json_formatter.h"

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <regex>
#include <securec.h>
#include <sstream>

#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_frame_format.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
const int FRAME_BUF_LEN = 1024;

#ifndef is_ohos_lite
bool DfxJsonFormatter::FormatJsonStack(std::string jsonStack, std::string& outStackStr)
{
    Json::Reader reader;
    Json::Value threads;
    if (!(reader.parse(jsonStack, threads))) {
        outStackStr.append("json not correct.");
        return false;
    }

    for (int i = 0; i < threads.size(); ++i) {
        Json::Value thread = threads[i];
        std::string name = thread["thread_name"].asString();
        std::string tid = thread["tid"].asString();
        std::ostringstream ss;
        ss << "Tid:" << tid << ", Name:" << name << "\n";
        const Json::Value frames = thread["frames"];
        for (int j = 0; j < frames.size(); ++j) {
            char buf[FRAME_BUF_LEN] = {0};
            char format[] = "#%02zu pc %s %s";

            std::string buildId = frames[j]["buildId"].asString();
            std::string file = frames[j]["file"].asString();
            std::string offset = frames[j]["offset"].asString();
            std::string pc = frames[j]["pc"].asString();
            std::string symbol = frames[j]["symbol"].asString();
            if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format,
                j,
                pc.c_str(),
                file.empty() ? "Unknown" : file.c_str()) <= 0) {
                outStackStr.append("json not correct.");
                return false;
            }
            ss << std::string(buf, strlen(buf));
            if (!symbol.empty()) {
                ss << "(";
                ss << symbol.c_str();
                ss << "+" << offset << ")";
            }
            if (!buildId.empty()) {
                ss << "(" << buildId << ")";
            }
            ss << std::endl;
        }
        outStackStr.append(ss.str());

    }
    return true;
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
