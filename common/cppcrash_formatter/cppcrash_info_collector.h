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
#ifndef CPPCRASH_INFO_COLLECTOR_H
#define CPPCRASH_INFO_COLLECTOR_H

#include <string>
#include <cstdint>
#include <vector>
#include "dfx_frame.h"

namespace OHOS {
namespace HiviewDFX {

struct ThreadInfo {
    std::string threadName;
    int32_t tid {0};
    std::vector<DfxFrame> frames;
};

struct HeadInfo {
    std::string buildInfo;
    std::string timestamp;
    int32_t pid {0};
    int32_t uid {0};
    std::string hiTraceId;
    std::string pname;
    std::string lifeTime;
    std::string rssMeminfo;
    std::string logSource;
    std::string reason;
    std::string lastFatalMessage;
    bool extendPcLrPrinting {false};
    std::string logCutOffSizeStr;
    bool simplifyVmaPrinting {false};
    bool mergeAppLog {false};
    bool minidumpLog {false};
    std::string appRunningUniqueId;
};

struct SignalInfo {
    int32_t signo {0};
    int32_t code {0};
    std::string address;
};

class CppCrashInfoCollector {
public:
    static CppCrashInfoCollector& Instance();

    CppCrashInfoCollector(const CppCrashInfoCollector&) = delete;
    CppCrashInfoCollector& operator=(const CppCrashInfoCollector&) = delete;

    void SetBuildInfo(const std::string& buildInfo);
    void SetTimestamp(const std::string& timestamp);
    void SetPid(int32_t pid);
    void SetUid(int32_t uid);
    void SetHiTraceId(const std::string& hiTraceId);
    void SetPname(const std::string& pname);
    void SetProcessLifeTime(const std::string& lifeTime);
    void SetProcessRssMeminfo(const std::string& rssMeminfo);
    void SetLogSource(const std::string& logSource);
    void SetReason(const std::string& reason);
    void SetSignal(int32_t signo, int32_t code, const std::string& address);
    void SetLastFatalMessage(const std::string& message);
    void SetKeyThread(const std::string& threadName, int32_t tid, const std::vector<DfxFrame>& frames);
    void AddOtherThread(const std::string& threadName, int32_t tid, const std::vector<DfxFrame>& frames);
    void SetSubmitterStacktrace(const std::string& stacktrace);
    void SetRegisters(const std::string& registers);
    void SetExtraCrashInfo(const std::string& extraCrashInfo);
    void SetMemoryNearRegister(const std::string& memoryNearRegister);
    void SetFaultStack(const std::string& faultStack);
    void SetMaps(const std::string& maps);
    void SetOpenFiles(const std::string& openFiles);
    void SetExtendPcLrPrinting(bool enable);
    void SetLogCutOffSizeStr(const std::string& sizeStr);
    void SetSimplifyVmaPrinting(bool enable);
    void SetMergeAppLog(bool enable);
    void SetMinidumpLog(bool enable);
    void SetAppRunningUniqueId(const std::string& appRunningUniqueId);
    void SetNeedFormatFlag(bool flag);
    void SetCrashInfoSize(size_t size);
    void Reset();

    const std::string& GetBuildInfo() const;
    const std::string& GetTimestamp() const;
    int32_t GetPid() const;
    int32_t GetUid() const;
    const std::string& GetHiTraceId() const;
    const std::string& GetPname() const;
    const std::string& GetProcessLifeTime() const;
    const std::string& GetProcessRssMeminfo() const;
    const std::string& GetLogSource() const;
    const std::string& GetReason() const;
    int32_t GetSignalSigno() const;
    int32_t GetSignalCode() const;
    const std::string& GetSignalAddress() const;
    const std::string& GetLastFatalMessage() const;
    const ThreadInfo& GetKeyThread() const;
    const std::vector<ThreadInfo>& GetOtherThreads() const;
    const std::string& GetSubmitterStacktrace() const;
    const std::string& GetRegisters() const;
    const std::string& GetExtraCrashInfo() const;
    const std::string& GetMemoryNearRegister() const;
    const std::string& GetFaultStack() const;
    const std::string& GetMaps() const;
    const std::string& GetOpenFiles() const;
    bool GetExtendPcLrPrinting() const;
    const std::string& GetLogCutOffSizeStr() const;
    bool GetSimplifyVmaPrinting() const;
    bool GetMergeAppLog() const;
    bool GetMinidumpLog() const;
    const std::string& GetAppRunningUniqueId() const;
    bool GetNeedFormatFlag() const;
    size_t GetCrashInfoSize() const;

private:
    CppCrashInfoCollector() = default;
    ~CppCrashInfoCollector() = default;

    HeadInfo headInfo_;
    SignalInfo signalInfo_;
    ThreadInfo keyThreadInfo_;
    std::vector<ThreadInfo> otherThreadInfos_;
    std::string submitterStacktrace_;
    std::string registers_;
    std::string extraCrashInfo_;
    std::string memoryNearRegister_;
    std::string faultStack_;
    std::string maps_;
    std::string openFiles_;
    bool needFormatFlag_ {false};
    size_t crashInfoSize_ {0};
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // CPPCRASH_INFO_COLLECTOR_H