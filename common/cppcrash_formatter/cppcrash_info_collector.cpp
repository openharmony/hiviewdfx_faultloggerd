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

#include "cppcrash_info_collector.h"

namespace OHOS {
namespace HiviewDFX {

CppCrashInfoCollector& CppCrashInfoCollector::Instance()
{
    static CppCrashInfoCollector inst;
    return inst;
}

void CppCrashInfoCollector::SetBuildInfo(const std::string& buildInfo)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.buildInfo = buildInfo;
}

void CppCrashInfoCollector::SetTimestamp(const std::string& timestamp)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.timestamp = timestamp;
}

void CppCrashInfoCollector::SetPid(int32_t pid)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.pid = pid;
}

void CppCrashInfoCollector::SetUid(int32_t uid)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.uid = uid;
}

void CppCrashInfoCollector::SetHiTraceId(const std::string& hiTraceId)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.hiTraceId = hiTraceId;
}

void CppCrashInfoCollector::SetPname(const std::string& pname)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.pname = pname;
}

void CppCrashInfoCollector::SetProcessLifeTime(const std::string& lifeTime)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.lifeTime = lifeTime;
}

void CppCrashInfoCollector::SetProcessRssMeminfo(const std::string& rssMeminfo)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.rssMeminfo = rssMeminfo;
}

void CppCrashInfoCollector::SetReason(const std::string& reason)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.reason = reason;
}

void CppCrashInfoCollector::SetSignal(int32_t signo, int32_t code, const std::string& address)
{
    if (!needFormatFlag_) {
        return;
    }
    signalInfo_.signo = signo;
    signalInfo_.code = code;
    signalInfo_.address = address;
}

void CppCrashInfoCollector::SetLastFatalMessage(const std::string& message)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.lastFatalMessage = message;
}

void CppCrashInfoCollector::SetKeyThread(const std::string& threadName, int32_t tid,
    const std::vector<DfxFrame>& frames)
{
    if (!needFormatFlag_) {
        return;
    }
    keyThreadInfo_.threadName = threadName;
    keyThreadInfo_.tid = tid;
    keyThreadInfo_.frames = frames;
}

void CppCrashInfoCollector::AddOtherThread(const std::string& threadName, int32_t tid,
    const std::vector<DfxFrame>& frames)
{
    if (!needFormatFlag_) {
        return;
    }
    ThreadInfo info;
    info.threadName = threadName;
    info.tid = tid;
    info.frames = frames;
    otherThreadInfos_.push_back(info);
}

void CppCrashInfoCollector::SetSubmitterStacktrace(const std::string& stacktrace)
{
    if (!needFormatFlag_) {
        return;
    }
    submitterStacktrace_ = stacktrace;
}

void CppCrashInfoCollector::SetRegisters(const std::string& registers)
{
    if (!needFormatFlag_) {
        return;
    }
    registers_ = registers;
}

void CppCrashInfoCollector::SetExtraCrashInfo(const std::string& extraCrashInfo)
{
    if (!needFormatFlag_) {
        return;
    }
    extraCrashInfo_ = extraCrashInfo;
}

void CppCrashInfoCollector::SetMemoryNearRegister(const std::string& memoryNearRegister)
{
    if (!needFormatFlag_) {
        return;
    }
    memoryNearRegister_ = memoryNearRegister;
}

void CppCrashInfoCollector::SetFaultStack(const std::string& faultStack)
{
    if (!needFormatFlag_) {
        return;
    }
    faultStack_ = faultStack;
}

void CppCrashInfoCollector::SetMaps(const std::string& maps)
{
    if (!needFormatFlag_) {
        return;
    }
    maps_ = maps;
}

void CppCrashInfoCollector::SetOpenFiles(const std::string& openFiles)
{
    if (!needFormatFlag_) {
        return;
    }
    openFiles_ = openFiles;
}

void CppCrashInfoCollector::SetExtendPcLrPrinting(bool enable)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.extendPcLrPrinting = enable;
}

void CppCrashInfoCollector::SetLogCutOffSizeStr(const std::string& sizeStr)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.logCutOffSizeStr = sizeStr;
}

void CppCrashInfoCollector::SetSimplifyVmaPrinting(bool enable)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.simplifyVmaPrinting = enable;
}

void CppCrashInfoCollector::SetMergeAppLog(bool enable)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.mergeAppLog = enable;
}

void CppCrashInfoCollector::SetMinidumpLog(bool enable)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.minidumpLog = enable;
}

void CppCrashInfoCollector::SetAppRunningUniqueId(const std::string& appRunningUniqueId)
{
    if (!needFormatFlag_) {
        return;
    }
    headInfo_.appRunningUniqueId = appRunningUniqueId;
}

void CppCrashInfoCollector::SetNeedFormatFlag(bool flag)
{
    needFormatFlag_ = flag;
}

void CppCrashInfoCollector::SetCrashInfoSize(size_t size)
{
    if (!needFormatFlag_) {
        return;
    }
    crashInfoSize_ = size;
}

const std::string& CppCrashInfoCollector::GetBuildInfo() const
{
    return headInfo_.buildInfo;
}

const std::string& CppCrashInfoCollector::GetTimestamp() const
{
    return headInfo_.timestamp;
}

int32_t CppCrashInfoCollector::GetPid() const
{
    return headInfo_.pid;
}

int32_t CppCrashInfoCollector::GetUid() const
{
    return headInfo_.uid;
}

const std::string& CppCrashInfoCollector::GetHiTraceId() const
{
    return headInfo_.hiTraceId;
}

const std::string& CppCrashInfoCollector::GetPname() const
{
    return headInfo_.pname;
}

const std::string& CppCrashInfoCollector::GetProcessLifeTime() const
{
    return headInfo_.lifeTime;
}

const std::string& CppCrashInfoCollector::GetProcessRssMeminfo() const
{
    return headInfo_.rssMeminfo;
}

const std::string& CppCrashInfoCollector::GetReason() const
{
    return headInfo_.reason;
}

int32_t CppCrashInfoCollector::GetSignalSigno() const
{
    return signalInfo_.signo;
}

int32_t CppCrashInfoCollector::GetSignalCode() const
{
    return signalInfo_.code;
}

const std::string& CppCrashInfoCollector::GetSignalAddress() const
{
    return signalInfo_.address;
}

const std::string& CppCrashInfoCollector::GetLastFatalMessage() const
{
    return headInfo_.lastFatalMessage;
}

const ThreadInfo& CppCrashInfoCollector::GetKeyThread() const
{
    return keyThreadInfo_;
}

const std::vector<ThreadInfo>& CppCrashInfoCollector::GetOtherThreads() const
{
    return otherThreadInfos_;
}

const std::string& CppCrashInfoCollector::GetSubmitterStacktrace() const
{
    return submitterStacktrace_;
}

const std::string& CppCrashInfoCollector::GetRegisters() const
{
    return registers_;
}

const std::string& CppCrashInfoCollector::GetExtraCrashInfo() const
{
    return extraCrashInfo_;
}

const std::string& CppCrashInfoCollector::GetMemoryNearRegister() const
{
    return memoryNearRegister_;
}

const std::string& CppCrashInfoCollector::GetFaultStack() const
{
    return faultStack_;
}

const std::string& CppCrashInfoCollector::GetMaps() const
{
    return maps_;
}

const std::string& CppCrashInfoCollector::GetOpenFiles() const
{
    return openFiles_;
}

bool CppCrashInfoCollector::GetExtendPcLrPrinting() const
{
    return headInfo_.extendPcLrPrinting;
}

const std::string& CppCrashInfoCollector::GetLogCutOffSizeStr() const
{
    return headInfo_.logCutOffSizeStr;
}

bool CppCrashInfoCollector::GetSimplifyVmaPrinting() const
{
    return headInfo_.simplifyVmaPrinting;
}

bool CppCrashInfoCollector::GetMergeAppLog() const
{
    return headInfo_.mergeAppLog;
}

bool CppCrashInfoCollector::GetMinidumpLog() const
{
    return headInfo_.minidumpLog;
}

const std::string& CppCrashInfoCollector::GetAppRunningUniqueId() const
{
    return headInfo_.appRunningUniqueId;
}

bool CppCrashInfoCollector::GetNeedFormatFlag() const
{
    return needFormatFlag_;
}

size_t CppCrashInfoCollector::GetCrashInfoSize() const
{
    return crashInfoSize_;
}

void CppCrashInfoCollector::Reset()
{
    headInfo_ = HeadInfo();
    signalInfo_ = SignalInfo();
    keyThreadInfo_ = ThreadInfo();
    otherThreadInfos_.clear();
    submitterStacktrace_.clear();
    registers_.clear();
    extraCrashInfo_.clear();
    memoryNearRegister_.clear();
    faultStack_.clear();
    maps_.clear();
    openFiles_.clear();
    needFormatFlag_ = false;
    crashInfoSize_ = 0;
}

} // namespace HiviewDFX
} // namespace OHOS