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

#include "cppcrash_formatter.h"

#ifndef is_ohos_lite
#include "cJSON.h"
#include "dfx_util.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_printf.h"
#include "string_util.h"
#include "dfx_map.h"
#endif

namespace OHOS {
namespace HiviewDFX {

#ifndef is_ohos_lite
bool CppCrashJsonFormatter::NeedFormatCrashInfo()
{
    return CppCrashInfoCollector::Instance().GetNeedFormatFlag();
}

std::string CppCrashJsonFormatter::FormatCrashInfo()
{
    if (!NeedFormatCrashInfo()) {
        return "";
    }
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) {
        DFXLOGE("Failed to create cJSON object for root");
        return "";
    }
    CppCrashInfoCollector& collector = CppCrashInfoCollector::Instance();
    cJSON* versionItem = cJSON_CreateString(CPPCRASH_JSON_VERSION_VALUE);
    if (versionItem == nullptr) {
        DFXLOGE("Failed to create cJSON string for version item");
        cJSON_Delete(root);
        return "";
    }
    cJSON_AddItemToObject(root, CPPCRASH_JSON_VERSION_KEY, versionItem);
    AddHeadInfo(root, collector);
    AddSignalInfo(root, collector);
    AddThreadInfo(root, collector);
    AddSubmitterStacktrace(root, collector);
    AddRegistersInfo(root, collector);
    AddExtraCrashInfo(root, collector);
    AddMemoryNearRegister(root, collector);
    AddFaultStack(root, collector);
    AddMapsInfo(root, collector);
    AddOpenFiles(root, collector);
    char* str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (str == nullptr) {
        DFXLOGE("Failed to print cJSON to string");
        return "";
    }
    std::string result(str);
    free(str);
    collector.SetCrashInfoSize(result.size());
    return result;
}

void CppCrashJsonFormatter::AddHeadInfo(cJSON* root, CppCrashInfoCollector& collector)
{
    if (root == nullptr) {
        DFXLOGE("AddHeadInfo: root is nullptr");
        return;
    }
    AddStringValue(root, "BUILD_INFO", collector.GetBuildInfo());
    AddStringValue(root, "TIMESTAMP", collector.GetTimestamp());
    cJSON* pidItem = cJSON_CreateNumber(collector.GetPid());
    cJSON* uidItem = cJSON_CreateNumber(collector.GetUid());
    if (pidItem != nullptr) {
        cJSON_AddItemToObject(root, "PID", pidItem);
    }
    if (uidItem != nullptr) {
        cJSON_AddItemToObject(root, "UID", uidItem);
    }
    AddStringValue(root, "HITRACEID", collector.GetHiTraceId());
    AddStringValue(root, "PNAME", collector.GetPname(), true);
    AddStringValue(root, "APP_RUNNING_UNIQUE_ID", collector.GetAppRunningUniqueId());
    AddStringValue(root, "PROCESS_LIFETIME", collector.GetProcessLifeTime());
    AddStringValue(root, "PROCESS_RSS_MEMINFO", collector.GetProcessRssMeminfo());
    AddStringValue(root, "REASON", collector.GetReason());
    AddStringValue(root, "LAST_FATAL_MESSAGE", collector.GetLastFatalMessage());

    cJSON* config = cJSON_CreateObject();
    if (config == nullptr) {
        DFXLOGE("Failed to create cJSON object for config");
        return;
    }
    AddBoolValue(config, "EXTEND_PC_LR_PRINTING", collector.GetExtendPcLrPrinting());
    AddStringValue(config, "LOG_CUT_OFF_SIZE", collector.GetLogCutOffSizeStr());
    AddBoolValue(config, "SIMPLIFY_MAPS_PRINTING", collector.GetSimplifyVmaPrinting());
    AddBoolValue(config, "MERGE_APP_LOG_PRINTING", collector.GetMergeAppLog());
    AddBoolValue(config, "ENABLE_MINIDUMP_LOG", collector.GetMinidumpLog());
    int size = cJSON_GetArraySize(config);
    if (size > 0) {
        cJSON_AddItemToObject(root, "ENABLED_APP_LOG_CONFIG", config);
    } else {
        cJSON_Delete(config);
    }
}

void CppCrashJsonFormatter::AddSignalInfo(cJSON* root, CppCrashInfoCollector& collector)
{
    if (root == nullptr) {
        DFXLOGE("AddSignalInfo: root is nullptr");
        return;
    }
    int32_t signo = collector.GetSignalSigno();
    cJSON* signal = cJSON_CreateObject();
    if (signal == nullptr) {
        DFXLOGE("Failed to create cJSON object for signal");
        return;
    }
    cJSON* signoItem = cJSON_CreateNumber(signo);
    cJSON* codeItem = cJSON_CreateNumber(collector.GetSignalCode());
    cJSON* addrItem = cJSON_CreateString(collector.GetSignalAddress().c_str());
    if (signoItem != nullptr) {
        cJSON_AddItemToObject(signal, "signo", signoItem);
    }
    if (codeItem != nullptr) {
        cJSON_AddItemToObject(signal, "code", codeItem);
    }
    if (addrItem != nullptr) {
        cJSON_AddItemToObject(signal, "address", addrItem);
    }
    cJSON_AddItemToObject(root, "SIGNAL", signal);
}

void CppCrashJsonFormatter::AddThreadInfo(cJSON* root, CppCrashInfoCollector& collector)
{
    if (root == nullptr) {
        DFXLOGE("AddThreadInfo: root is nullptr");
        return;
    }
    const ThreadInfo& keyThread = collector.GetKeyThread();
    if (!keyThread.frames.empty() || !keyThread.threadName.empty() || keyThread.tid != 0) {
        cJSON* keyThreadJson = FillThreadJson(keyThread);
        if (keyThreadJson != nullptr) {
            cJSON_AddItemToObject(root, "KEY_THREAD_INFO", keyThreadJson);
        }
    }
    const std::vector<ThreadInfo>& otherThreads = collector.GetOtherThreads();
    if (otherThreads.empty()) {
        return;
    }
    cJSON* threadsArray = cJSON_CreateArray();
    if (threadsArray == nullptr) {
        DFXLOGE("Failed to create cJSON array for threads");
        return;
    }
    for (const auto& thread : otherThreads) {
        cJSON* threadJson = FillThreadJson(thread);
        if (threadJson != nullptr) {
            cJSON_AddItemToArray(threadsArray, threadJson);
        }
    }
    cJSON_AddItemToObject(root, "OTHER_THREAD_INFO", threadsArray);
}

void CppCrashJsonFormatter::AddSubmitterStacktrace(cJSON* root, CppCrashInfoCollector& collector)
{
    AddStringValue(root, "SUBMITTER_STACKTRACE", collector.GetSubmitterStacktrace());
}

void CppCrashJsonFormatter::AddRegistersInfo(cJSON* root, CppCrashInfoCollector& collector)
{
    AddStringValue(root, "KEY_THREAD_REGISTERS", collector.GetRegisters());
}

void CppCrashJsonFormatter::AddExtraCrashInfo(cJSON* root, CppCrashInfoCollector& collector)
{
    AddStringValue(root, "EXTRA_CRASH_INFO", collector.GetExtraCrashInfo());
}

void CppCrashJsonFormatter::AddMemoryNearRegister(cJSON* root, CppCrashInfoCollector& collector)
{
    AddStringValue(root, "MEMORY_NEAR_REGISTERS", collector.GetMemoryNearRegister());
}

void CppCrashJsonFormatter::AddFaultStack(cJSON* root, CppCrashInfoCollector& collector)
{
    AddStringValue(root, "FAULT_STACK", collector.GetFaultStack());
}

void CppCrashJsonFormatter::AddMapsInfo(cJSON* root, CppCrashInfoCollector& collector)
{
    AddStringValue(root, "PROCESS_MAPS", collector.GetMaps());
}

void CppCrashJsonFormatter::AddOpenFiles(cJSON* root, CppCrashInfoCollector& collector)
{
    AddStringValue(root, "OPEN_FILES", collector.GetOpenFiles());
}

bool CppCrashJsonFormatter::AddStringValue(cJSON* root, const char* key, const std::string& value, bool isRequire)
{
    if (root == nullptr || key == nullptr) {
        return false;
    }
    if (value.empty() && !isRequire) {
        return false;
    }
    cJSON* item = cJSON_CreateString(value.c_str());
    if (item == nullptr) {
        DFXLOGE("Failed to create cJSON string for key:%{public}s", key);
        return false;
    }
    cJSON_AddItemToObject(root, key, item);
    return true;
}

bool CppCrashJsonFormatter::AddBoolValue(cJSON* root, const char* key, bool value)
{
    if (root == nullptr || key == nullptr || !value) {
        return false;
    }
    cJSON* item = cJSON_CreateBool(value);
    if (item == nullptr) {
        DFXLOGE("Failed to create cJSON bool for key:%{public}s", key);
        return false;
    }
    cJSON_AddItemToObject(root, key, item);
    return true;
}

cJSON* CppCrashJsonFormatter::FillThreadJson(const ThreadInfo& threadInfo)
{
    cJSON* threadJson = cJSON_CreateObject();
    if (threadJson == nullptr) {
        DFXLOGE("Failed to create cJSON object for thread");
        return nullptr;
    }
    cJSON* nameItem = cJSON_CreateString(threadInfo.threadName.c_str());
    cJSON* tidItem = cJSON_CreateNumber(threadInfo.tid);
    if (nameItem != nullptr) {
        cJSON_AddItemToObject(threadJson, "thread_name", nameItem);
    }
    if (tidItem != nullptr) {
        cJSON_AddItemToObject(threadJson, "tid", tidItem);
    }
    cJSON* framesJson = FillFramesJson(threadInfo.frames);
    if (framesJson != nullptr) {
        cJSON_AddItemToObject(threadJson, "frames", framesJson);
    }
    return threadJson;
}

cJSON* CppCrashJsonFormatter::FillFramesJson(const std::vector<DfxFrame>& frames)
{
    cJSON* framesArray = cJSON_CreateArray();
    if (framesArray == nullptr) {
        DFXLOGE("Failed to create cJSON array for frames");
        return nullptr;
    }
    for (const auto& frame : frames) {
        cJSON* frameJson = frame.isJsFrame ? FillJsFrameJson(frame) : FillNativeFrameJson(frame);
        if (frameJson != nullptr) {
            cJSON_AddItemToArray(framesArray, frameJson);
        }
#if defined(__aarch64__)
        if (!frame.isJsFrame && IsLastValidFrame(frame)) {
            break;
        }
#endif
    }
    return framesArray;
}

cJSON* CppCrashJsonFormatter::FillNativeFrameJson(const DfxFrame& frame)
{
    cJSON* frameJson = cJSON_CreateObject();
    if (frameJson == nullptr) {
        DFXLOGE("Failed to create cJSON object for native frame");
        return nullptr;
    }
    std::string pcStr = FormatPc(frame.relPc);
    cJSON* pcItem = cJSON_CreateString(pcStr.c_str());
    std::string funcName;
    if (frame.funcName.length() <= MAX_FUNC_NAME_LEN) {
        funcName = frame.funcName;
    }
    cJSON* symItem = cJSON_CreateString(funcName.c_str());
    cJSON* offsetItem = cJSON_CreateNumber(frame.funcOffset);
    std::string strippedMapName = DfxMap::UnFormatMapName(frame.mapName);
    cJSON* fileItem = cJSON_CreateString(strippedMapName.c_str());
    cJSON* buildIdItem = cJSON_CreateString(frame.buildId.c_str());
    if (pcItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "pc", pcItem);
    }
    if (symItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "symbol", symItem);
    }
    if (offsetItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "offset", offsetItem);
    }
    if (fileItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "file", fileItem);
    }
    if (buildIdItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "buildId", buildIdItem);
    }
    return frameJson;
}

cJSON* CppCrashJsonFormatter::FillJsFrameJson(const DfxFrame& frame)
{
    cJSON* frameJson = cJSON_CreateObject();
    if (frameJson == nullptr) {
        DFXLOGE("Failed to create cJSON object for js frame");
        return nullptr;
    }
    cJSON* fileItem = cJSON_CreateString(frame.mapName.c_str());
    cJSON* pkgItem = cJSON_CreateString(frame.packageName.c_str());
    cJSON* symItem = cJSON_CreateString(frame.funcName.c_str());
    cJSON* lineItem = cJSON_CreateNumber(frame.line);
    cJSON* colItem = cJSON_CreateNumber(frame.column);
    if (fileItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "file", fileItem);
    }
    if (pkgItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "packageName", pkgItem);
    }
    if (symItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "symbol", symItem);
    }
    if (lineItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "line", lineItem);
    }
    if (colItem != nullptr) {
        cJSON_AddItemToObject(frameJson, "column", colItem);
    }
    return frameJson;
}

std::string CppCrashJsonFormatter::FormatPc(uint64_t relPc)
{
#ifdef __LP64__
    return StringPrintf("%016lx", relPc);
#else
    return StringPrintf("%08llx", relPc);
#endif
}

bool CppCrashJsonFormatter::IsLastValidFrame(const DfxFrame& frame)
{
    static uintptr_t libcStartPc = 0;
    static uintptr_t libffrtStartEntry = 0;
    if (((libcStartPc != 0) && (frame.pc == libcStartPc)) ||
        ((libffrtStartEntry != 0) && (frame.pc == libffrtStartEntry))) {
        return true;
    }

    if (frame.mapName.find("ld-musl-aarch64.so.1") != std::string::npos &&
        frame.funcName.find("start") != std::string::npos) {
        libcStartPc = frame.pc;
        return true;
    }

    if (frame.mapName.find("libffrt") != std::string::npos &&
        frame.funcName.find("CoStartEntry") != std::string::npos) {
        libffrtStartEntry = frame.pc;
        return true;
    }
    return false;
}
#else
std::string CppCrashTextFormatter::FormatCrashInfo()
{
    return "";
}

bool CppCrashTextFormatter::NeedFormatCrashInfo()
{
    return false;
}
#endif

ICrashFormatter& CppCrashFormatterFactory::Create()
{
#ifndef is_ohos_lite
    static CppCrashJsonFormatter instance;
#else
    static CppCrashTextFormatter instance;
#endif
    return instance;
}

} // namespace HiviewDFX
} // namespace OHOS
