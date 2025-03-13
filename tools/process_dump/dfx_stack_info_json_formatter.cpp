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

#include "dfx_stack_info_json_formatter.h"

#include <cinttypes>
#include <string>

#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "process_dumper.h"
#if defined(__aarch64__)
#include "printer.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
static const char NATIVE_CRASH_TYPE[] = "NativeCrash";

#ifndef is_ohos_lite
void FillJsFrame(const DfxFrame& frame, Json::Value& jsonInfo)
{
    Json::Value frameJson;
    frameJson["file"] = frame.mapName;
    frameJson["packageName"] = frame.packageName;
    frameJson["symbol"] = frame.funcName;
    frameJson["line"] = frame.line;
    frameJson["column"] = frame.column;
    jsonInfo.append(frameJson);
}
#endif
}

bool DfxStackInfoJsonFormatter::GetJsonFormatInfo(bool isDump, std::string& jsonStringInfo,
    const ProcessDumpRequest& request, DfxProcess& process) const
{
#ifndef is_ohos_lite
    Json::Value jsonInfo;
    isDump ? GetDumpJsonFormatInfo(jsonInfo, process) : GetCrashJsonFormatInfo(jsonInfo, request, process);
    jsonStringInfo.append(Json::FastWriter().write(jsonInfo));
    return true;
#endif
    return false;
}

#ifndef is_ohos_lite
void DfxStackInfoJsonFormatter::GetCrashJsonFormatInfo(Json::Value& jsonInfo, const ProcessDumpRequest& request,
    DfxProcess& process) const
{
    jsonInfo["time"] = request.timeStamp;
    jsonInfo["uuid"] = "";
    jsonInfo["crash_type"] = NATIVE_CRASH_TYPE;
    jsonInfo["pid"] = process.processInfo_.pid;
    jsonInfo["uid"] = process.processInfo_.uid;
    jsonInfo["app_running_unique_id"] = request.appRunningId;

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
    if (process.keyThread_ == nullptr) {
        exception["thread_name"] = "";
        exception["tid"] = 0;
    } else {
        exception["thread_name"] = process.keyThread_->threadInfo_.threadName;
        exception["tid"] = process.keyThread_->threadInfo_.tid;
        FillFrames(*process.keyThread_, frames);
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

void DfxStackInfoJsonFormatter::GetDumpJsonFormatInfo(Json::Value& jsonInfo, DfxProcess& process) const
{
    Json::Value thread;
    Json::Value frames(Json::arrayValue);
    if (process.keyThread_ == nullptr) {
        thread["thread_name"] = "";
        thread["tid"] = 0;
    } else {
        thread["thread_name"] = process.keyThread_->threadInfo_.threadName;
        thread["tid"] = process.keyThread_->threadInfo_.tid;
        FillFrames(*process.keyThread_, frames);
    }
    thread["frames"] = frames;
    jsonInfo.append(thread);

    // fill other thread info
    const auto& otherThreads = process.GetOtherThreads();
    if (otherThreads.size() > 0) {
        AppendThreads(otherThreads, jsonInfo);
    }
}

bool DfxStackInfoJsonFormatter::FillFrames(DfxThread& thread, Json::Value& jsonInfo)
{
    const auto& threadFrames = thread.GetFrames();
    for (const auto& frame : threadFrames) {
        if (frame.isJsFrame) {
            FillJsFrame(frame, jsonInfo);
            continue;
        }
        FillNativeFrame(frame, jsonInfo);
#if defined(__aarch64__)
        if (Printer::IsLastValidFrame(frame)) {
            break;
        }
#endif
    }
    return true;
}

void DfxStackInfoJsonFormatter::FillNativeFrame(const DfxFrame& frame, Json::Value& jsonInfo)
{
    Json::Value frameJson;
#ifdef __LP64__
    frameJson["pc"] = StringPrintf("%016lx", frame.relPc);
#else
    frameJson["pc"] = StringPrintf("%08llx", frame.relPc);
#endif
    if (frame.funcName.length() > MAX_FUNC_NAME_LEN) {
        DFXLOGD("length of funcName greater than 256 byte, do not report it");
        frameJson["symbol"] = "";
    } else {
        frameJson["symbol"] = frame.funcName;
    }
    frameJson["offset"] = frame.funcOffset;
    std::string strippedMapName = DfxMap::UnFormatMapName(frame.mapName);
    frameJson["file"] = strippedMapName;
    frameJson["buildId"] = frame.buildId;
    jsonInfo.append(frameJson);
}

void DfxStackInfoJsonFormatter::AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads,
                                              Json::Value& jsonInfo) const
{
    for (auto const& oneThread : threads) {
        if (oneThread != nullptr) {
            Json::Value threadJson;
            threadJson["thread_name"] = oneThread->threadInfo_.threadName;
            threadJson["tid"] = oneThread->threadInfo_.tid;
            Json::Value frames(Json::arrayValue);
            FillFrames(*oneThread, frames);
            threadJson["frames"] = frames;
            jsonInfo.append(threadJson);
        }
    }
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
