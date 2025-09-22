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
    std::string& jsonStringInfo)
{
#ifndef is_ohos_lite
    Json::Value jsonInfo;
    switch (request.type) {
        case ProcessDumpType::DUMP_TYPE_CPP_CRASH:
            GetCrashJsonFormatInfo(request, process, jsonInfo);
            break;
        case ProcessDumpType::DUMP_TYPE_DUMP_CATCH:
            GetDumpJsonFormatInfo(process, jsonInfo);
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
void DumpInfoJsonFormatter::GetCrashJsonFormatInfo(const ProcessDumpRequest& request, DfxProcess& process,
    Json::Value& jsonInfo)
{
    jsonInfo["time"] = request.timeStamp;
    jsonInfo["uuid"] = "";
    jsonInfo["crash_type"] = "NativeCrash";
    jsonInfo["process_name"] = process.GetProcessInfo().processName;
    jsonInfo["pid"] = process.GetProcessInfo().pid;
    jsonInfo["uid"] = process.GetProcessInfo().uid;
    jsonInfo["app_running_unique_id"] = request.appRunningUniqueId;

    DfxSignal dfxSignal(request.siginfo.si_signo);
    Json::Value signal;
    signal["signo"] = request.siginfo.si_signo;
    signal["code"] = request.siginfo.si_code;
    signal["address"] = dfxSignal.IsAddrAvailable() ?
        StringPrintf("%" PRIX64_ADDR, reinterpret_cast<uint64_t>(request.siginfo.si_addr)) : "";
    Json::Value exception;
    exception["signal"] = signal;
    exception["message"] = process.GetFatalMessage();

    Json::Value frames(Json::arrayValue);
    if (process.GetKeyThread() == nullptr) {
        exception["thread_name"] = "";
        exception["tid"] = 0;
    } else {
        exception["thread_name"] = process.GetKeyThread()->GetThreadInfo().threadName;
        exception["tid"] = process.GetKeyThread()->GetThreadInfo().tid;
        FillFramesJson(process.GetKeyThread()->GetFrames(), frames);
    }
    exception["frames"] = frames;
    jsonInfo["exception"] = exception;

    // fill other thread info
    const auto& otherThreads = process.GetOtherThreads();
    if (otherThreads.size() > 0) {
        Json::Value threadsJsonArray(Json::arrayValue);
        AppendThreads(otherThreads, threadsJsonArray);
        jsonInfo["threads"] = threadsJsonArray;
    }
}

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
                                          Json::Value& jsonInfo) const
{
    for (auto const& oneThread : threads) {
        if (oneThread != nullptr) {
            Json::Value threadJson;
            threadJson["thread_name"] = oneThread->GetThreadInfo().threadName;
            threadJson["tid"] = oneThread->GetThreadInfo().tid;
            Json::Value framesJson(Json::arrayValue);
            FillFramesJson(oneThread->GetFrames(), framesJson);
            threadJson["frames"] = framesJson;
            jsonInfo.append(threadJson);
        }
    }
}

bool DumpInfoJsonFormatter::FillFramesJson(const std::vector<DfxFrame>& frames, Json::Value& jsonInfo) const
{
    for (const auto& frame : frames) {
        if (frame.isJsFrame) {
            FillJsFrameJson(frame, jsonInfo);
        } else {
            FillNativeFrameJson(frame, jsonInfo);
#if defined(__aarch64__)
            if (DumpUtils::IsLastValidFrame(frame)) {
                break;
            }
#endif
        }
    }
    return true;
}

void DumpInfoJsonFormatter::FillJsFrameJson(const DfxFrame& frame, Json::Value& jsonInfo) const
{
    Json::Value frameJson;
    frameJson["file"] = frame.mapName;
    frameJson["packageName"] = frame.packageName;
    frameJson["symbol"] = frame.funcName;
    frameJson["line"] = frame.line;
    frameJson["column"] = frame.column;
    jsonInfo.append(frameJson);
}

void DumpInfoJsonFormatter::FillNativeFrameJson(const DfxFrame& frame, Json::Value& jsonInfo) const
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
#endif
} // namespace HiviewDFX
} // namespace OHOS
