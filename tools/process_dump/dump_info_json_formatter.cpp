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

#include "dump_info_json_formatter.h"

#include <cinttypes>
#include <string>

#include "cppcrash_utils.h"
#include "dfx_define.h"

#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "dump_utils.h"
#include "process_dumper.h"

namespace OHOS {
namespace HiviewDFX {

bool DumpInfoJsonFormatter::GetJsonFormatInfo(const ProcessDumpRequest& request, DfxProcess& process,
    std::string& jsonStringInfo, int dumpError)
{
#ifndef is_ohos_lite
    Json::Value jsonInfo;
    Json::Value thread;
    switch (request.type) {
        case ProcessDumpType::DUMP_TYPE_DUMP_CATCH:
            jsonInfo["dump_result"] = dumpError;
            GetDumpJsonFormatInfo(process, thread);
            jsonInfo["dump_threads"] = thread;
            break;
        default:
            break;
    }
    jsonStringInfo.append(Json::FastWriter().write(jsonInfo));
    return true;
#endif
    return false;
}

#ifndef is_ohos_lite
void DumpInfoJsonFormatter::GetDumpJsonFormatInfo(DfxProcess& process, Json::Value& jsonInfo)
{
    Json::Value thread;
    Json::Value frames(Json::arrayValue);
    if (process.GetKeyThread() == nullptr) {
        thread["thread_name"] = "";
        thread["tid"] = 0;
    } else {
        thread["thread_name"] = process.GetKeyThread()->GetThreadInfo().threadName;
        thread["tid"] = process.GetKeyThread()->GetThreadInfo().tid;
        FillThreadstatInfo(thread, process.GetKeyThread()->GetProcessInfo());
        FillFramesJson(process.GetKeyThread()->GetFrames(), frames);
    }
    thread["frames"] = frames;
    jsonInfo.append(thread);

    // fill other thread info
    const auto& otherThreads = process.GetOtherThreads();
    if (otherThreads.size() > 0) {
        AppendThreads(otherThreads, jsonInfo);
    }
}

void DumpInfoJsonFormatter::AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads,
                                          Json::Value& jsonInfo)
{
    for (auto const& oneThread : threads) {
        if (oneThread != nullptr) {
            Json::Value threadJson;
            threadJson["thread_name"] = oneThread->GetThreadInfo().threadName;
            threadJson["tid"] = oneThread->GetThreadInfo().tid;
            FillThreadstatInfo(threadJson, oneThread->GetProcessInfo());
            Json::Value framesJson(Json::arrayValue);
            FillFramesJson(oneThread->GetFrames(), framesJson);
            threadJson["frames"] = framesJson;
            jsonInfo.append(threadJson);
        }
    }
}

bool DumpInfoJsonFormatter::FillFramesJson(const std::vector<DfxFrame>& frames, Json::Value& jsonInfo)
{
    for (const auto& frame : frames) {
        if (frame.isJsFrame) {
            FillJsFrameJson(frame, jsonInfo);
        } else {
            FillNativeFrameJson(frame, jsonInfo);
#if defined(__aarch64__)
            if (CppCrashUtils::IsLastValidFrame(frame)) {
                break;
            }
#endif
        }
    }
    return true;
}

void DumpInfoJsonFormatter::FillJsFrameJson(const DfxFrame& frame, Json::Value& jsonInfo)
{
    Json::Value frameJson;
    frameJson["file"] = frame.mapName;
    frameJson["packageName"] = frame.packageName;
    frameJson["symbol"] = frame.funcName;
    frameJson["line"] = frame.line;
    frameJson["column"] = frame.column;
    jsonInfo.append(frameJson);
}

void DumpInfoJsonFormatter::FillNativeFrameJson(const DfxFrame& frame, Json::Value& jsonInfo)
{
    Json::Value frameJson;
#ifdef __LP64__
    frameJson["pc"] = StringPrintf("%016lx", frame.relPc);
#else
    frameJson["pc"] = StringPrintf("%08llx", frame.relPc);
#endif
    if (frame.funcName.length() <= MAX_FUNC_NAME_LEN) {
        frameJson["symbol"] = frame.funcName;
    } else {
        frameJson["symbol"] = "";
    }
    frameJson["offset"] = frame.funcOffset;
    std::string strippedMapName = DfxMap::UnFormatMapName(frame.mapName);
    frameJson["file"] = strippedMapName;
    frameJson["buildId"] = frame.buildId;
    jsonInfo.append(frameJson);
}

void DumpInfoJsonFormatter::FillThreadstatInfo(Json::Value& jsonInfo, const std::shared_ptr<ProcessInfo> info)
{
    if (info == nullptr) {
        return;
    }
    jsonInfo["state"] = std::string(1, static_cast<char>(info->state));
    jsonInfo["utime"] = info->utime;
    jsonInfo["stime"] = info->stime;
    jsonInfo["priority"] = info->priority;
    jsonInfo["nice"] = info->nice;
    jsonInfo["clk"] = GetClkTck();
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
