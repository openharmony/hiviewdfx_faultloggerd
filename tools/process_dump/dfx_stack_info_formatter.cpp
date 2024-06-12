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

#include "dfx_stack_info_formatter.h"

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
    frameJson["symbol"] = frame.funcName;
    frameJson["line"] = frame.line;
    frameJson["column"] = frame.column;
    jsonInfo.append(frameJson);
}
#endif
}

bool DfxStackInfoFormatter::GetStackInfo(bool isJsonDump, std::string& jsonStringInfo) const
{
    bool result = false;
#ifndef is_ohos_lite
    DFXLOG_DEBUG("GetStackInfo isJsonDump:%d", isJsonDump);
    Json::Value jsonInfo;
    if (!GetStackInfo(isJsonDump, jsonInfo)) {
        return result;
    }
    jsonStringInfo.append(Json::FastWriter().write(jsonInfo));
    result = true;
#endif
    return result;
}

#ifndef is_ohos_lite
bool DfxStackInfoFormatter::GetStackInfo(bool isJsonDump, Json::Value& jsonInfo) const
{
    if ((process_ == nullptr) || (request_ == nullptr)) {
        DFXLOG_ERROR("%s", "GetStackInfo var is null");
        return false;
    }
    if (isJsonDump) {
        GetDumpInfo(jsonInfo);
    } else {
        GetNativeCrashInfo(jsonInfo);
    }
    return true;
}

void DfxStackInfoFormatter::GetNativeCrashInfo(Json::Value& jsonInfo) const
{
    jsonInfo["time"] = request_->timeStamp;
    jsonInfo["uuid"] = "";
    jsonInfo["crash_type"] = NATIVE_CRASH_TYPE;
    jsonInfo["pid"] = process_->processInfo_.pid;
    jsonInfo["uid"] = process_->processInfo_.uid;
    jsonInfo["app_running_unique_id"] = request_->appRunningId;

    DfxSignal dfxSignal(request_->siginfo.si_signo);
    Json::Value signal;
    signal["signo"] = request_->siginfo.si_signo;
    signal["code"] = request_->siginfo.si_code;
    if (dfxSignal.IsAddrAvailable()) {
#if defined(__LP64__)
        signal["address"] = StringPrintf("%#018lx", reinterpret_cast<uint64_t>(request_->siginfo.si_addr));
#else
        signal["address"] = StringPrintf("%#010llx", reinterpret_cast<uint64_t>(request_->siginfo.si_addr));
#endif
    } else {
        signal["address"] = "";
    }
    Json::Value exception;
    exception["signal"] = signal;
    exception["message"] = process_->GetFatalMessage();
    exception["thread_name"] = process_->keyThread_->threadInfo_.threadName;
    exception["tid"] = process_->keyThread_->threadInfo_.tid;
    Json::Value frames(Json::arrayValue);
    if (process_->vmThread_ != nullptr) {
        FillFrames(process_->vmThread_, frames);
    } else {
        FillFrames(process_->keyThread_, frames);
    }
    exception["frames"] = frames;
    jsonInfo["exception"] = exception;

    // fill other thread info
    auto otherThreads = process_->GetOtherThreads();
    if (otherThreads.size() > 0) {
        Json::Value threadsJsonArray(Json::arrayValue);
        AppendThreads(otherThreads, threadsJsonArray);
        jsonInfo["threads"] = threadsJsonArray;
    }
}

void DfxStackInfoFormatter::GetDumpInfo(Json::Value& jsonInfo) const
{
    Json::Value thread;
    thread["thread_name"] = process_->keyThread_->threadInfo_.threadName;
    thread["tid"] = process_->keyThread_->threadInfo_.tid;
    Json::Value frames(Json::arrayValue);
    FillFrames(process_->keyThread_, frames);
    thread["frames"] = frames;
    jsonInfo.append(thread);

    // fill other thread info
    auto otherThreads = process_->GetOtherThreads();
    if (otherThreads.size() > 0) {
        AppendThreads(otherThreads, jsonInfo);
    }
}

bool DfxStackInfoFormatter::FillFrames(const std::shared_ptr<DfxThread>& thread,
                                       Json::Value& jsonInfo) const
{
    if (thread == nullptr) {
        DFXLOG_ERROR("%s", "FillFrames thread is null");
        return false;
    }
    const auto& threadFrames = thread->GetFrames();
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

void DfxStackInfoFormatter::FillNativeFrame(const DfxFrame& frame, Json::Value& jsonInfo) const
{
    Json::Value frameJson;
#ifdef __LP64__
    frameJson["pc"] = StringPrintf("%016lx", frame.relPc);
#else
    frameJson["pc"] = StringPrintf("%08llx", frame.relPc);
#endif
    if (frame.funcName.length() > MAX_FUNC_NAME_LEN) {
        DFXLOG_DEBUG("%s", "length of funcName greater than 256 byte, do not report it");
        frameJson["symbol"] = "";
    } else {
        frameJson["symbol"] = frame.funcName;
    }
    frameJson["offset"] = frame.funcOffset;
    std::string strippedMapName = frame.mapName;
    DfxMaps::UnFormatMapName(strippedMapName);
    frameJson["file"] = strippedMapName;
    frameJson["buildId"] = frame.buildId;
    jsonInfo.append(frameJson);
}

void DfxStackInfoFormatter::AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads,
                                          Json::Value& jsonInfo) const
{
    for (auto const& oneThread : threads) {
        Json::Value threadJson;
        threadJson["thread_name"] = oneThread->threadInfo_.threadName;
        threadJson["tid"] = oneThread->threadInfo_.tid;
        Json::Value frames(Json::arrayValue);
        FillFrames(oneThread, frames);
        threadJson["frames"] = frames;
        jsonInfo.append(threadJson);
    }
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
