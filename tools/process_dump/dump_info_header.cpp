/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "decorative_dump_info.h"
#include <cinttypes>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include "dfx_buffer_writer.h"
#include "dfx_log.h"
#include "dfx_signal.h"
#include "dfx_util.h"
#include "dfx_cutil.h"
#include "dump_utils.h"
#include "crash_exception.h"
#include "string_printf.h"
#ifndef is_ohos_lite
#include "parameter.h"
#include "parameters.h"
#include "hitrace/hitracechainc.h"
#endif
#include "procinfo.h"
#include "info/fatal_message.h"
#include "cppcrash_info_collector.h"
namespace OHOS {
namespace HiviewDFX {
namespace {
#ifndef PAGE_SIZE
constexpr size_t PAGE_SIZE = 4096;
#endif
}
REGISTER_DUMP_INFO_CLASS(DumpInfoHeader);

void DumpInfoHeader::Collect(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    std::string tempStr;
    if (request.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
#ifndef is_ohos_lite
        tempStr = DumpUtils::GetBuildInfo();
        headerInfo_ = "Build info:" + tempStr + "\n";
        CppCrashInfoCollector::Instance().SetBuildInfo(tempStr);
#endif
        headerInfo_ += GetCrashLogConfigInfo(request, process);
    }
    tempStr = GetCurrentTimeStr(request.timeStamp);
    headerInfo_ += "Timestamp:" + tempStr;
    CppCrashInfoCollector::Instance().SetTimestamp(tempStr);
    headerInfo_ += StringPrintf("Pid:%d\nUid:%d\n", process.GetProcessInfo().pid, process.GetProcessInfo().uid);
    CppCrashInfoCollector::Instance().SetPid(process.GetProcessInfo().pid);
    CppCrashInfoCollector::Instance().SetUid(process.GetProcessInfo().uid);
#ifndef is_ohos_lite
    if (request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH && request.hitraceId.valid == HITRACE_ID_VALID) {
        tempStr = StringPrintf("%" PRIx64 "\n", static_cast<uint64_t>(request.hitraceId.chainId));
        headerInfo_ += "HiTraceId:" + tempStr;
        CppCrashInfoCollector::Instance().SetHiTraceId(tempStr);
    }
#endif
    CollectProcessInfo(process, request, unwinder);
}

void DumpInfoHeader::CollectProcessInfo(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    std::string tempStr;
    headerInfo_ += StringPrintf("Process name:%s\n", process.GetProcessInfo().processName.c_str());
    CppCrashInfoCollector::Instance().SetPname(process.GetProcessInfo().processName);
    CppCrashInfoCollector::Instance().SetAppRunningUniqueId(request.appRunningUniqueId);
    if (request.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        headerInfo_ += StringPrintf("App running unique id:%s\n", request.appRunningUniqueId);
        uint64_t lifeTimeSeconds = 0;
        int errCode = GetProcessLifeCycle(process.GetProcessInfo().pid, lifeTimeSeconds);
        process.SetLifeTime(lifeTimeSeconds);
        tempStr = std::to_string(lifeTimeSeconds) + "s";
        headerInfo_ += ("Process life time:" + tempStr + "\n");
        CppCrashInfoCollector::Instance().SetProcessLifeTime(tempStr);
        if (errCode != 0) {
            DFXLOGE("Get process lifeCycle fail, errCode: %{public}d", errCode);
            ReportCrashException(CrashExceptionCode::CRASH_LOG_EPROCESS_LIFECYCLE);
        }
        uint64_t rss = GetProcRssMemInfo(process.GetProcessInfo().pid);
        process.SetRss(rss);
        tempStr = StringPrintf("%" PRIu64 "(Rss)\n", rss);
        headerInfo_ += "Process Memory(kB):" + tempStr;
        CppCrashInfoCollector::Instance().SetProcessRssMeminfo(tempStr);
        tempStr = GetReasonInfo(request, process, *unwinder.GetMaps());
        headerInfo_ += ("Reason:" + tempStr);
        CppCrashInfoCollector::Instance().SetReason(tempStr);
        DfxSignal dfxSignal(request.siginfo.si_signo);
        tempStr = dfxSignal.IsAddrAvailable() ?
            StringPrintf("%" PRIX64_ADDR, reinterpret_cast<uint64_t>(request.siginfo.si_addr)) : "";
        CppCrashInfoCollector::Instance().SetSignal(request.siginfo.si_signo, request.siginfo.si_code, tempStr);
        process.AppendFatalMessage(GetLastFatalMsg(process, request));
        auto msg = process.GetFatalMessage();
        if (!msg.empty()) {
            headerInfo_ += "LastFatalMessage:" + msg + "\n";
            CppCrashInfoCollector::Instance().SetLastFatalMessage(msg);
        }
    }
}

void DumpInfoHeader::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DfxBufferWriter::GetInstance().AppendBriefDumpInfo(headerInfo_);
    DfxBufferWriter::GetInstance().WriteMsg(headerInfo_);
    DecorativeDumpInfo::Print(process, request, unwinder);
}

std::string DumpInfoHeader::GetReasonInfo(const ProcessDumpRequest& request, DfxProcess& process, DfxMaps& maps)
{
    std::string reasonInfo = DfxSignal::PrintSignal(request.siginfo);
    do {
        uint64_t addr = reinterpret_cast<uint64_t>(request.siginfo.si_addr);
        if (request.siginfo.si_signo == SIGSEGV &&
            (request.siginfo.si_code == SEGV_MAPERR || request.siginfo.si_code == SEGV_ACCERR)) {
            if (addr < PAGE_SIZE) {
                reasonInfo += " probably caused by NULL pointer dereference";
                break;
            }
            if (process.GetKeyThread() == nullptr) {
                DFXLOGW("%{public}s is nullptr", "keyThread_");
                break;
            }
            std::vector<std::shared_ptr<DfxMap>> map;
            std::string elfName;
            if (process.GetKeyThread()->GetThreadInfo().tid == process.GetProcessInfo().pid) {
                elfName = "[stack]";
            } else {
                elfName = StringPrintf("[anon:stack:%d]", process.GetKeyThread()->GetThreadInfo().tid);
            }
            if (!maps.FindMapsByName(elfName, map) || map[0] == nullptr) {
                break;
            }
            if (addr < map[0]->begin && map[0]->begin - addr <= PAGE_SIZE) {
                reasonInfo += StringPrintf(
                    " current thread stack low address = %" PRIX64_ADDR ", probably caused by stack-buffer-overflow",
                    map[0]->begin);
            }
        } else if (request.siginfo.si_signo == SIGSYS && request.siginfo.si_code == SYS_SECCOMP) {
            reasonInfo += StringPrintf(" syscall number is %d", request.siginfo.si_syscall);
        }
    } while (false);
    reasonInfo += "\n";
    process.SetReason(reasonInfo);
    return reasonInfo;
}

std::string DumpInfoHeader::GetCrashLogConfigInfo(const ProcessDumpRequest& request, DfxProcess& process)
{
    constexpr uint32_t extendPcLrMask = 0x1;
    constexpr uint32_t simplifyVmaMask = 0x2;
    constexpr uint32_t mergeAppLogMask = 0x4;
    CrashLogConfig crashLogConfig;
    std::string crashLogConfigInfo;
    if ((request.crashLogConfig & extendPcLrMask) == extendPcLrMask) {
        crashLogConfig.extendPcLrPrinting = true;
        crashLogConfigInfo += "Extend pc lr printing:true\n";
        CppCrashInfoCollector::Instance().SetExtendPcLrPrinting(true);
    }
    // 32 : cutoff size start from high 32 bit
    uint32_t logFileCutoffSizeBytes = static_cast<uint32_t>(request.crashLogConfig >> 32);
    std::string logCutSizeStr;
    if (logFileCutoffSizeBytes != 0) {
        crashLogConfig.logFileCutoffSizeBytes = logFileCutoffSizeBytes;
        logCutSizeStr = StringPrintf("%" PRIu32 "B\n", crashLogConfig.logFileCutoffSizeBytes);
        crashLogConfigInfo += "Log cut off size:" + logCutSizeStr;
        CppCrashInfoCollector::Instance().SetLogCutOffSizeStr(logCutSizeStr);
    }
    if ((request.crashLogConfig & simplifyVmaMask) == simplifyVmaMask) {
        crashLogConfig.simplifyVmaPrinting = true;
        crashLogConfigInfo += "Simplify maps printing:true\n";
        CppCrashInfoCollector::Instance().SetSimplifyVmaPrinting(true);
    }
    if ((request.crashLogConfig & mergeAppLogMask) == mergeAppLogMask) {
        crashLogConfig.mergeAppLog = true;
        crashLogConfigInfo += "Merge app log printing:true\n";
        CppCrashInfoCollector::Instance().SetMergeAppLog(true);
    }
    if (IsMiniDumpEnable(request.crashLogConfig)) {
        crashLogConfig.minidumpLog = true;
        crashLogConfigInfo += "Enable minidump log:true\n";
        CppCrashInfoCollector::Instance().SetMinidumpLog(true);
    }
    if (!crashLogConfigInfo.empty()) {
        crashLogConfigInfo = "Enabled app log configs:\n" + crashLogConfigInfo;
    }
    process.SetCrashLogConfig(crashLogConfig);
    return crashLogConfigInfo;
}

std::string DumpInfoHeader::GetLastFatalMsg(const DfxProcess& process, const ProcessDumpRequest& request)
{
    std::string lastFatalMsg = "";
    if ((request.msg.type == MESSAGE_FATAL || request.msg.type == MESSAGE_CALLBACK) &&
         request.msg.addr != 0) {
        size_t len = request.msg.type == MESSAGE_FATAL ? MAX_FATAL_MSG_SIZE : MAX_CALLBACK_MSG_SIZE;
        lastFatalMsg += DumpUtils::ReadStringByPtrace(process.GetVmPid(), request.msg.addr, len);
    }
    lastFatalMsg += UpdateFatalMessageWhenDebugSignal(process, request);
    lastFatalMsg += ReadCrashObjString(request);
    return lastFatalMsg;
}

std::string DumpInfoHeader::ReadCrashObjString(const ProcessDumpRequest& request)
{
    std::string content = "";
#ifdef __LP64__
    if (request.type != ProcessDumpType::DUMP_TYPE_CPP_CRASH || request.crashObj == 0) {
        DFXLOGI("crash obj not int.");
        return content;
    }
    uintptr_t type = request.crashObj >> 56; // 56 :: Move 56 bit to the right
    uintptr_t addr = request.crashObj & 0xffffffffffffff;
    if (type == 0) {
        DFXLOGI("Start read string type of crashObj.");
        content = DumpUtils::ReadStringByPtrace(request.nsPid, addr, MAX_FATAL_MSG_SIZE);
    }
#endif
    return content;
}

std::string DumpInfoHeader::UpdateFatalMessageWhenDebugSignal(const DfxProcess& process,
    const ProcessDumpRequest& request)
{
    if (request.type != ProcessDumpType::DUMP_TYPE_BADFD && request.type != ProcessDumpType::DUMP_TYPE_FDSAN &&
        request.type != ProcessDumpType::DUMP_TYPE_JEMALLOC &&
        request.type != ProcessDumpType::DUMP_TYPE_ARKTS_ENVSAN) {
        return "";
    }
    pid_t pid = process.GetVmPid() != 0 ? process.GetVmPid() : request.nsPid;
    if (pid == 0) {
        DFXLOGE("invalid pid, return directly!");
        return "";
    }

    auto debugMsgPtr = reinterpret_cast<uintptr_t>(request.siginfo.si_value.sival_ptr);
    debug_msg_t dMsg = {0};
    if (ReadProcMemByPid(pid, debugMsgPtr, &dMsg, sizeof(dMsg)) != sizeof(debug_msg_t)) {
        DFXLOGE("Get debug_msg_t failed.");
        return "";
    }
    auto msgPtr = reinterpret_cast<uintptr_t>(dMsg.msg);
    auto debugMsg = DumpUtils::ReadStringByPtrace(pid, msgPtr, MAX_FATAL_MSG_SIZE);
    if (debugMsg.empty()) {
        DFXLOGE("Get debug_msg_t.msg failed.");
        return "";
    }
    return debugMsg;
}
}
}
