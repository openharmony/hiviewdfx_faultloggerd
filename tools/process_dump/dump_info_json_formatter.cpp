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
    cJSON *jsonInfo = nullptr;
    switch (request.type) {
        case ProcessDumpType::DUMP_TYPE_CPP_CRASH:
            jsonInfo = cJSON_CreateObject();
            if (!jsonInfo) {
                DFXLOGE("Create cJson jsonInfo object failed.");
                return false;
            }
            GetCrashJsonFormatInfo(request, process, jsonInfo);
            break;
        case ProcessDumpType::DUMP_TYPE_DUMP_CATCH:
            jsonInfo = cJSON_CreateArray();
            if (!jsonInfo) {
                DFXLOGE("Create cJson jsonInfo array failed.");
                return false;
            }
            GetDumpJsonFormatInfo(process, jsonInfo);
            break;
        default:
            return false;
    }
    auto itemStr = cJSON_PrintUnformatted(jsonInfo);
    jsonStringInfo = itemStr;
    cJSON_free(itemStr);
    cJSON_Delete(jsonInfo);
    return true;
#endif
    return false;
}

#ifndef is_ohos_lite
bool DumpInfoJsonFormatter::AddItemToJsonInfo(const ProcessDumpRequest& request, DfxProcess& process,
    cJSON *jsonInfo)
{
    DfxSignal dfxSignal(request.siginfo.si_signo);
    cJSON *signal = cJSON_CreateObject();
    if (!signal) {
        DFXLOGE("Create cJson signal object failed.");
        return false;
    }
    cJSON_AddNumberToObject(signal, "signo", request.siginfo.si_signo);
    cJSON_AddNumberToObject(signal, "code", request.siginfo.si_code);
    std::string address = dfxSignal.IsAddrAvailable() ?
        StringPrintf("%" PRIX64_ADDR, reinterpret_cast<uint64_t>(request.siginfo.si_addr)) : "";
    cJSON_AddStringToObject(signal, "address", address.c_str());
    cJSON *exception = cJSON_CreateObject();
    if (!exception) {
        DFXLOGE("Create cJson exception object failed.");
        cJSON_Delete(signal);
        return false;
    }
    cJSON_AddItemToObject(exception, "signal", signal);
    cJSON_AddStringToObject(exception, "message", process.GetFatalMessage().c_str());

    cJSON *frames = cJSON_CreateArray();
    if (!frames) {
        DFXLOGE("Create cJson exception array failed.");
        cJSON_Delete(exception);
        return false;
    }
    if (process.GetKeyThread() == nullptr) {
        cJSON_AddStringToObject(exception, "thread_name", "");
        cJSON_AddNumberToObject(exception, "tid", 0);
    } else {
        cJSON_AddStringToObject(exception, "thread_name", process.GetKeyThread()->GetThreadInfo().threadName.c_str());
        cJSON_AddNumberToObject(exception, "tid", process.GetKeyThread()->GetThreadInfo().tid);
        FillFramesJson(process.GetKeyThread()->GetFrames(), frames);
    }
    cJSON_AddItemToObject(exception, "frames", frames);
    cJSON_AddItemToObject(jsonInfo, "exception", exception);
    return true;
}

void DumpInfoJsonFormatter::GetCrashJsonFormatInfo(const ProcessDumpRequest& request, DfxProcess& process,
    cJSON *jsonInfo)
{
    cJSON_AddNumberToObject(jsonInfo, "time", request.timeStamp);
    cJSON_AddStringToObject(jsonInfo, "uuid", "");
    cJSON_AddStringToObject(jsonInfo, "crash_type", "NativeCrash");
    cJSON_AddNumberToObject(jsonInfo, "pid", process.GetProcessInfo().pid);
    cJSON_AddNumberToObject(jsonInfo, "uid", process.GetProcessInfo().uid);
    cJSON_AddStringToObject(jsonInfo, "app_running_unique_id", request.appRunningId);

    if (!AddItemToJsonInfo(request, process, jsonInfo)) {
        return;
    }

    // fill other thread info
    const auto& otherThreads = process.GetOtherThreads();
    if (otherThreads.size() > 0) {
        cJSON *threadsJsonArray = cJSON_CreateArray();
        if (!threadsJsonArray) {
            DFXLOGE("Create cJson threadsJsonArray array failed.");
            return;
        }
        AppendThreads(otherThreads, threadsJsonArray);
        cJSON_AddItemToObject(jsonInfo, "threads", threadsJsonArray);
    }
}

void DumpInfoJsonFormatter::GetDumpJsonFormatInfo(DfxProcess& process, cJSON *jsonInfo)
{
    cJSON *thread = cJSON_CreateObject();
    cJSON *frames = cJSON_CreateArray();
    if (!thread || !frames) {
        DFXLOGE("Create cJson thread or frames failed.");
        cJSON_Delete(thread);
        cJSON_Delete(frames);
        return;
    }
    if (process.GetKeyThread() == nullptr) {
        cJSON_AddStringToObject(thread, "thread_name", "");
        cJSON_AddNumberToObject(thread, "tid", 0);
    } else {
        cJSON_AddStringToObject(thread, "thread_name", process.GetKeyThread()->GetThreadInfo().threadName.c_str());
        cJSON_AddNumberToObject(thread, "tid", process.GetKeyThread()->GetThreadInfo().tid);
        FillFramesJson(process.GetKeyThread()->GetFrames(), frames);
    }
    cJSON_AddItemToObject(thread, "frames", frames);
    cJSON_AddItemToArray(jsonInfo, thread);

    // fill other thread info
    const auto& otherThreads = process.GetOtherThreads();
    if (otherThreads.size() > 0) {
        AppendThreads(otherThreads, jsonInfo);
    }
}

void DumpInfoJsonFormatter::AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads,
                                          cJSON *jsonInfo) const
{
    for (auto const& oneThread : threads) {
        if (oneThread != nullptr) {
            cJSON *threadJson = cJSON_CreateObject();
            if (!threadJson) {
                DFXLOGE("Create cJson threadJson object failed, continue.");
                continue;
            }
            cJSON_AddStringToObject(threadJson, "thread_name", oneThread->GetThreadInfo().threadName.c_str());
            cJSON_AddNumberToObject(threadJson, "tid", oneThread->GetThreadInfo().tid);
            cJSON *framesJson = cJSON_CreateArray();
            if (!framesJson) {
                DFXLOGE("Create cJson framesJson array failed, continue.");
                cJSON_Delete(threadJson);
                continue;
            }
            FillFramesJson(oneThread->GetFrames(), framesJson);
            cJSON_AddItemToObject(threadJson, "frames", framesJson);
            cJSON_AddItemToArray(jsonInfo, threadJson);
        }
    }
}

bool DumpInfoJsonFormatter::FillFramesJson(const std::vector<DfxFrame>& frames, cJSON *jsonInfo) const
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

void DumpInfoJsonFormatter::FillJsFrameJson(const DfxFrame& frame, cJSON *jsonInfo) const
{
    cJSON *frameJson = cJSON_CreateObject();
    if (!frameJson) {
        DFXLOGE("Create cJson frameJson object failed.");
        return;
    }
    cJSON_AddStringToObject(frameJson, "file", frame.mapName.c_str());
    cJSON_AddStringToObject(frameJson, "packageName", frame.packageName.c_str());
    cJSON_AddStringToObject(frameJson, "symbol", frame.funcName.c_str());
    cJSON_AddNumberToObject(frameJson, "line", frame.line);
    cJSON_AddNumberToObject(frameJson, "column", frame.column);
    cJSON_AddItemToArray(jsonInfo, frameJson);
}

void DumpInfoJsonFormatter::FillNativeFrameJson(const DfxFrame& frame, cJSON *jsonInfo) const
{
    cJSON *frameJson = cJSON_CreateObject();
    if (!frameJson) {
        DFXLOGE("Create cJson frameJson object failed.");
        return;
    }
#ifdef __LP64__
    std::string pc = StringPrintf("%016lx", frame.relPc);
#else
    std::string pc = StringPrintf("%08llx", frame.relPc);
#endif
    cJSON_AddStringToObject(frameJson, "pc", pc.c_str());
    if (frame.funcName.length() > MAX_FUNC_NAME_LEN) {
        DFXLOGD("length of funcName greater than 256 byte, do not report it");
        cJSON_AddStringToObject(frameJson, "symbol", "");
    } else {
        cJSON_AddStringToObject(frameJson, "symbol", frame.funcName.c_str());
    }
    cJSON_AddNumberToObject(frameJson, "offset", frame.funcOffset);
    std::string strippedMapName = DfxMap::UnFormatMapName(frame.mapName);
    cJSON_AddStringToObject(frameJson, "file", strippedMapName.c_str());
    cJSON_AddStringToObject(frameJson, "buildId", frame.buildId.c_str());
    cJSON_AddItemToArray(jsonInfo, frameJson);
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
