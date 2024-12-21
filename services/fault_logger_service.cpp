/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "fault_logger_service.h"

#include <fstream>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <memory>

#include "dfx_define.h"
#include "faultloggerd_socket.h"
#include "dfx_trace.h"
#include "temp_file_manager.h"
#include "dfx_log.h"
#include "string_printf.h"
#include "dfx_util.h"

#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif

#ifndef is_ohos_lite
#include "fault_logger_pipe.h"
#endif

namespace OHOS {
namespace HiviewDFX {

namespace {

constexpr const char* const FAULTLOGGERD_SERVICE_TAG = "FAULTLOGGERD_SERVICE";

bool GetUcredByPeerCred(struct ucred& rcred, int32_t connectionFd)
{
    socklen_t credSize = sizeof(rcred);
    int err = getsockopt(connectionFd, SOL_SOCKET, SO_PEERCRED, &rcred, &credSize);
    if (err != 0) {
        DFXLOGE("%{public}s :: Failed to GetCredential, errno(%{public}d)", FAULTLOGGERD_SERVICE_TAG, errno);
        return false;
    }
    return true;
}

bool CheckCallerUID(uint32_t callerUid)
{
    constexpr uint32_t rootUid = 0;
    constexpr uint32_t bmsUid = 1000;
    constexpr uint32_t hiviewUid = 1201;
    constexpr uint32_t hidumperServiceUid = 1212;
    constexpr uint32_t foundationUid = 5523;
    // If caller's is BMS / root or caller's uid/pid is validate, just return true
    if ((callerUid == bmsUid) ||
        (callerUid == rootUid) ||
        (callerUid == hiviewUid) ||
        (callerUid == hidumperServiceUid) ||
        (callerUid == foundationUid)) {
        return true;
    }
    DFXLOGW("%{public}s :: CheckCallerUID :: Caller Uid(%{public}d) is unexpectly.\n",
            FAULTLOGGERD_SERVICE_TAG, callerUid);
    return false;
}

#ifndef is_ohos_lite

std::map<int32_t, int64_t> crashTimeMap_{};

void ClearTimeOutRecords()
{
#ifdef FAULTLOGGERD_TEST
    constexpr int validTime = 1;
#else
    constexpr int validTime = 8;
#endif
    auto currentTime = time(nullptr);
    for (auto it = crashTimeMap_.begin(); it != crashTimeMap_.end();) {
        if ((it->second + validTime) <= currentTime) {
            crashTimeMap_.erase(it++);
        } else {
            it++;
        }
    }
}

bool IsCrashed(int32_t pid)
{
    DFX_TRACE_SCOPED("IsCrashed");
    ClearTimeOutRecords();
    return crashTimeMap_.find(pid) != crashTimeMap_.end();
}

void RecordFileCreation(int32_t type, int32_t pid)
{
    if (type == static_cast<int32_t>(FaultLoggerType::CPP_CRASH)) {
        ClearTimeOutRecords();
        crashTimeMap_[pid] = time(nullptr);
    }
}

std::map<int, std::unique_ptr<FaultLoggerPipePair>> sdkDumpPipesRecord_{};

void RecordSdkDumpPipe(int pid, uint64_t time, bool isJson)
{
    if (sdkDumpPipesRecord_.find(pid) == sdkDumpPipesRecord_.end()) {
        sdkDumpPipesRecord_.emplace(pid, std::make_unique<FaultLoggerPipePair>(time, isJson));
    }
}

bool CheckSdkDumpRecord(int pid, uint64_t time)
{
    auto iter = sdkDumpPipesRecord_.find(pid);
    if (iter != sdkDumpPipesRecord_.end()) {
        const int pipTimeOut = 10000;
        if (iter->second && time > iter->second->time_ && time - iter->second->time_ > pipTimeOut) {
            sdkDumpPipesRecord_.erase(iter);
            return false;
        }
        return true;
    }
    return false;
}

FaultLoggerPipePair* GetSdkDumpPipePair(int pid)
{
    auto iter = sdkDumpPipesRecord_.find(pid);
    if (iter == sdkDumpPipesRecord_.end()) {
        return nullptr;
    }
    return iter->second.get();
}

void DelSdkDumpPipePair(int pid)
{
    auto iter = sdkDumpPipesRecord_.find(pid);
    if (iter != sdkDumpPipesRecord_.end()) {
        sdkDumpPipesRecord_.erase(iter);
    }
}
#endif

bool CheckRequestCredential(int32_t connectionFd, int32_t requestPid)
{
    struct ucred creds{};
    if (!GetUcredByPeerCred(creds, connectionFd)) {
        return false;
    }
    if (CheckCallerUID(creds.uid)) {
        return true;
    }
    if (creds.pid != requestPid) {
        DFXLOGW("Failed to check request credential request:%{public}d:" \
            " cred:%{public}d fd:%{public}d",
                requestPid, creds.pid, connectionFd);
        return false;
    }
    return true;
}
}

#ifndef HISYSEVENT_DISABLE
bool FaultLoggeExceptionReportService::Filter(int32_t connectionFd, const CrashDumpException& requestData)
{
    if (strlen(requestData.message) == 0) {
        return false;
    }
    struct ucred creds{};
    if (!GetUcredByPeerCred(creds, connectionFd) || creds.uid != static_cast<uid_t>(requestData.uid)) {
        DFXLOGW("Failed to check request credential request uid:%{public}d:" \
            " cred uid:%{public}d fd:%{public}d",
                requestData.uid, creds.uid, connectionFd);
        return false;
    }
    return true;
}

int32_t FaultLoggeExceptionReportService::OnRequest(const std::string& socketName,
                                                    int32_t connectionFd,
                                                    const CrashDumpException& requestData)
{
    if (!Filter(connectionFd, requestData)) {
        return ResponseCode::REQUEST_REJECT;
    }
    HiSysEventWrite(
        HiSysEvent::Domain::RELIABILITY,
        "CPP_CRASH_EXCEPTION",
        HiSysEvent::EventType::FAULT,
        "PID", requestData.pid,
        "UID", requestData.uid,
        "HAPPEN_TIME", requestData.time,
        "ERROR_CODE", requestData.error,
        "ERROR_MSG", requestData.message);
    return ResponseCode::REQUEST_SUCCESS;
}

void FaultLoggerdStatsService::RemoveTimeoutDumpStats()
{
    constexpr uint64_t timeout = 10000;
    uint64_t now = GetTimeMilliSeconds();
    for (auto it = stats_.begin(); it != stats_.end();) {
        if (((now > it->signalTime) && (now - it->signalTime > timeout)) ||
            (now <= it->signalTime)) {
            it = stats_.erase(it);
        } else {
            ++it;
        }
    }
}

void FaultLoggerdStatsService::ReportDumpStats(const DumpStats& stat)
{
    HiSysEventWrite(
        HiSysEvent::Domain::HIVIEWDFX,
        "DUMP_CATCHER_STATS",
        HiSysEvent::EventType::STATISTIC,
        "CALLER_PROCESS_NAME", stat.callerProcessName,
        "CALLER_FUNC_NAME", stat.callerElfName,
        "TARGET_PROCESS_NAME", stat.targetProcessName,
        "RESULT", stat.result,
        "SUMMARY", stat.summary, // we need to parse summary when interface return false
        "PID", stat.pid,
        "REQUEST_TIME", stat.requestTime,
        "OVERALL_TIME", stat.dumpCatcherFinishTime - stat.requestTime,
        "SIGNAL_TIME", stat.signalTime - stat.requestTime,
        "DUMPER_START_TIME", stat.processdumpStartTime - stat.signalTime,
        "UNWIND_TIME", stat.processdumpFinishTime - stat.processdumpStartTime);
}

std::string FaultLoggerdStatsService::GetElfName(const FaultLoggerdStatsRequest& request)
{
    if (strlen(request.callerElf) > NAME_BUF_LEN) {
        return "";
    }
    return StringPrintf("%s(%p)", request.callerElf, reinterpret_cast<void*>(request.offset));
}

int32_t FaultLoggerdStatsService::OnRequest(const std::string& socketName, int32_t connectionFd,
                                            const FaultLoggerdStatsRequest& requestData)
{
    DFXLOGI("%{public}s :: %{public}s: HandleDumpStats", FAULTLOGGERD_SERVICE_TAG, __func__);
    size_t index = 0;
    bool hasRecord = false;
    for (index = 0; index < stats_.size(); index++) {
        if (stats_[index].pid == requestData.pid) {
            hasRecord = true;
            break;
        }
    }

    DumpStats stats;
    if (requestData.type == PROCESS_DUMP && !hasRecord) {
        stats.pid = requestData.pid;
        stats.signalTime = requestData.signalTime;
        stats.processdumpStartTime = requestData.processdumpStartTime;
        stats.processdumpFinishTime = requestData.processdumpFinishTime;
        stats.targetProcessName = requestData.targetProcess;
        stats_.emplace_back(stats);
    } else if (requestData.type == DUMP_CATCHER && hasRecord) {
        stats_[index].requestTime = requestData.requestTime;
        stats_[index].dumpCatcherFinishTime = requestData.dumpCatcherFinishTime;
        stats_[index].callerElfName = GetElfName(requestData);
        stats_[index].callerProcessName = requestData.callerProcess;
        stats_[index].result = requestData.result;
        stats_[index].summary = requestData.summary;
        ReportDumpStats(stats_[index]);
        stats_.erase(stats_.begin() + index);
    } else if (requestData.type == DUMP_CATCHER) {
        stats.pid = requestData.pid;
        stats.requestTime = requestData.requestTime;
        stats.dumpCatcherFinishTime = requestData.dumpCatcherFinishTime;
        stats.callerElfName = GetElfName(requestData);
        stats.result = requestData.result;
        stats.callerProcessName = requestData.callerProcess;
        stats.summary = requestData.summary;
        stats.targetProcessName = requestData.targetProcess;
        ReportDumpStats(stats);
    }
    RemoveTimeoutDumpStats();
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
    return responseData;
}
#endif

int32_t FaultLoggedFileDesService::OnRequest(const std::string& socketName,
                                             int32_t connectionFd,
                                             const FaultLoggerdRequest& requestData)
{
    DFX_TRACE_SCOPED("HandleDefaultClientRequest");
    if (!Filter(socketName, connectionFd, requestData)) {
        return ResponseCode::REQUEST_REJECT;
    }

    SmartFd sFd(TempFileManager::CreateFileDescriptor(requestData.type, requestData.pid,
                                                      requestData.tid, requestData.time));
    if (sFd < 0) {
        DFXLOGE("[%{public}d]: FaultLoggedFileDesService :: Failed to create log file, errno(%{public}d)", __LINE__,
                errno);
        return ResponseCode::ABNORMAL_SERVICE;
    }
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
#ifndef is_ohos_lite
    RecordFileCreation(requestData.type, requestData.pid);
#endif
    SendFileDescriptorToSocket(connectionFd, sFd);
    return responseData;
}

bool FaultLoggedFileDesService::Filter(const std::string& socketName, int32_t connectionFd,
                                       const FaultLoggerdRequest& requestData)
{
    switch (requestData.type) {
        case FaultLoggerType::CPP_CRASH:
        case FaultLoggerType::CPP_STACKTRACE:
        case FaultLoggerType::LEAK_STACKTRACE:
        case FaultLoggerType::JIT_CODE_LOG:
            return socketName == SERVER_CRASH_SOCKET_NAME;
        default:
            return CheckRequestCredential(connectionFd, requestData.pid);
    }
}

#ifndef is_ohos_lite
int32_t FaultloggerdSdkDumpService::Filter(const std::string& socketName,
    const SdkDumpRequestData& requestData, uint32_t uid)
{
    if (requestData.pid <= 0 || socketName != SERVER_SDKDUMP_SOCKET_NAME || !CheckCallerUID(uid)) {
        DFXLOGE("%{public}s :: HandleSdkDumpRequest :: pid(%{public}d) or socketName(%{public}s) fail.",
            FAULTLOGGERD_SERVICE_TAG, requestData.pid, socketName.c_str());
        return ResponseCode::REQUEST_REJECT;
    }
    if (IsCrashed(requestData.pid)) {
        DFXLOGW("%{public}s :: pid(%{public}d) has been crashed, break.\n",
                FAULTLOGGERD_SERVICE_TAG, requestData.pid);
        return ResponseCode::SDK_PROCESS_CRASHED;
    }
    if (CheckSdkDumpRecord(requestData.pid, requestData.time)) {
        DFXLOGE("%{public}s :: pid(%{public}d) is dumping, break.\n", FAULTLOGGERD_SERVICE_TAG, requestData.pid);
        return ResponseCode::SDK_DUMP_REPEAT;
    }
    return ResponseCode::REQUEST_SUCCESS;
}

int32_t FaultloggerdSdkDumpService::OnRequest(const std::string& socketName,
                                              int32_t connectionFd,
                                              const SdkDumpRequestData& requestData)
{
    DFX_TRACE_SCOPED("HandleSdkDumpRequest");
    DFXLOGI("Receive dump request for pid:%{public}d tid:%{public}d.", requestData.pid, requestData.tid);
    struct ucred creds;
    if (!GetUcredByPeerCred(creds, connectionFd)) {
        DFXLOGE("Sdk dump pid(%{public}d) request failed to get cred.", requestData.pid);
        return ResponseCode::REQUEST_REJECT;
    }
    int32_t responseCode = Filter(socketName, requestData, creds.uid);
    if (responseCode != ResponseCode::REQUEST_SUCCESS) {
        return responseCode;
    }

    /*
    *           all     threads my user, local pid             my user, remote pid     other user's process
    * 3rd       Y       Y(in signal_handler local)     Y(in signal_handler loacl)      N
    * system    Y       Y(in signal_handler local)     Y(in signal_handler loacl)      Y(in signal_handler remote)
    * root      Y       Y(in signal_handler local)     Y(in signal_handler loacl)      Y(in signal_handler remote)
    */

    /*
    * 1. pid != 0 && tid != 0:    means we want dump a thread, so we send signal to a thread.
        Main thread stack is tid's stack, we need ignore other thread info.
    * 2. pid != 0 && tid == 0:    means we want dump a process, so we send signal to process.
        Main thead stack is pid's stack, we need other tread info.
    */

    /*
     * in signal_handler we need to check caller pid and tid(which is send to signal handler by SYS_rt_sig.).
     * 1. caller pid == signal pid, means we do back trace in ourself process, means local backtrace.
     *      |- we do all tid back trace in signal handler's local unwind.
     * 2. pid != signal pid, means we do remote back trace.
     */

    /*
     * in local back trace, all unwind stack will save to signal_handler global var.(mutex lock in signal handler.)
     * in remote back trace, all unwind stack will save to file, and read in dump_catcher, then return.
     */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
    // defined in out/hi3516dv300/obj/third_party/musl/intermidiates/linux/musl_src_ported/include/signal.h
    siginfo_t si{0};
    si.si_signo = SIGDUMP;
    si.si_errno = 0;
    si.si_value.sival_int = requestData.tid;
    if (requestData.tid == 0 && sizeof(void*) == 8) { // 8 : platform 64
        si.si_value.sival_ptr = reinterpret_cast<void*>(requestData.endTime | (1ULL << 63)); // 63 : platform 64
    }
    si.si_code = requestData.sigCode;
    si.si_pid = static_cast<int32_t>(creds.pid);
    si.si_uid = static_cast<uid_t>(requestData.callerTid);
#pragma clang diagnostic pop
    /*
     * means we need dump all the threads in a process
     * --------
     * Accroding to the linux manual, A process-directed signal may be delivered to any one of the
     * threads that does not currently have the signal blocked.
     */
    RecordSdkDumpPipe(requestData.pid, requestData.time, requestData.isJson);
#ifndef FAULTLOGGERD_TEST
    if (syscall(SYS_rt_sigqueueinfo, requestData.pid, si.si_signo, &si) != 0) {
        DFXLOGE("Failed to SYS_rt_sigqueueinfo signal(%{public}d), errno(%{public}d).", si.si_signo, errno);
        DelSdkDumpPipePair(requestData.pid);
        return ResponseCode::SDK_DUMP_NOPROC;
    }
#endif
    int32_t res = ResponseCode::REQUEST_SUCCESS;
    SendMsgToSocket(connectionFd, &res, sizeof(res));
    return res;
}

bool FaultLoggerdPipService::Filter(const std::string &socketName, int32_t connectionFd,
                                    const PipFdRequestData &requestData)
{
    if (requestData.pipeType < FaultLoggerPipeType::PIPE_FD_READ_BUF ||
        requestData.pipeType > FaultLoggerPipeType::PIPE_FD_DELETE) {
        return false;
    }
    if (socketName == SERVER_CRASH_SOCKET_NAME) {
        return true;
    }
    return CheckRequestCredential(connectionFd, requestData.pid);
}

int32_t FaultLoggerdPipService::OnRequest(const std::string& socketName,
                                          int32_t connectionFd,
                                          const PipFdRequestData& requestData)
{
    DFX_TRACE_SCOPED("HandlePipeFdClientRequest");
    if (!Filter(socketName, connectionFd, requestData)) {
        return ResponseCode::REQUEST_REJECT;
    }
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    if (requestData.pipeType == FaultLoggerPipeType::PIPE_FD_DELETE) {
        DelSdkDumpPipePair(requestData.pid);
        SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
        return responseData;
    }
    DFXLOGD("%{public}s :: pid(%{public}d), pipeType(%{public}d).", FAULTLOGGERD_SERVICE_TAG,
            requestData.pid, requestData.pipeType);

    FaultLoggerPipePair* faultLoggerPipe = GetSdkDumpPipePair(requestData.pid);
    if (faultLoggerPipe == nullptr) {
        DFXLOGE("%{public}s :: cannot find pipe fd for pid(%{public}d).", FAULTLOGGERD_SERVICE_TAG, requestData.pid);
        return ResponseCode::ABNORMAL_SERVICE;
    }
    constexpr int32_t pipTypeMask = 0b1;
    constexpr int32_t pipUsageMask = 0b10;
    constexpr int32_t isJsonMask = 0b100;
    int fd = faultLoggerPipe->GetPipFd(requestData.pipeType & pipTypeMask, requestData.pipeType & pipUsageMask,
        requestData.pipeType & isJsonMask);
    if (fd < 0) {
        DFXLOGE("%{public}s :: Failed to get pipe fd, pipeType(%{public}d)",
                FAULTLOGGERD_SERVICE_TAG, requestData.pipeType);
        return ResponseCode::ABNORMAL_SERVICE;
    }
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
    SendFileDescriptorToSocket(connectionFd, fd);
    return responseData;
}
#endif
}
}