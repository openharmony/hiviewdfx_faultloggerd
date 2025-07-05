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

#include <algorithm>
#include <fstream>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_socket.h"
#include "fault_common_util.h"
#include "procinfo.h"
#include "proc_util.h"

#ifndef is_ohos_lite
#include "fault_logger_pipe.h"
#endif

#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif

#include "string_printf.h"
#include "temp_file_manager.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr const char* const FAULTLOGGERD_SERVICE_TAG = "FAULT_LOGGER_SERVICE";
bool CheckCallerUID(uint32_t callerUid)
{
    const uint32_t whitelist[] = {
        0, // rootUid
        1000, // bmsUid
        1201, // hiviewUid
        1212, // hidumperServiceUid
        5523, // foundationUid
    };
    if (std::find(std::begin(whitelist), std::end(whitelist), callerUid) == std::end(whitelist)) {
        DFXLOGW("%{public}s :: CheckCallerUID :: Caller Uid(%{public}d) is unexpectly.",
                FAULTLOGGERD_SERVICE_TAG, callerUid);
        return false;
    }
    return true;
}

bool CheckRequestCredential(int32_t connectionFd, int32_t requestPid)
{
    struct ucred creds{};
    if (!FaultCommonUtil::GetUcredByPeerCred(creds, connectionFd)) {
        return false;
    }
    if (CheckCallerUID(creds.uid)) {
        return true;
    }
    if (creds.pid != requestPid) {
        DFXLOGW("Failed to check request credential request:%{public}d: cred:%{public}d fd:%{public}d",
                requestPid, creds.pid, connectionFd);
        return false;
    }
    return true;
}
}

#ifndef HISYSEVENT_DISABLE
bool ExceptionReportService::Filter(int32_t connectionFd, const CrashDumpException& requestData)
{
    if (strlen(requestData.message) == 0) {
        return false;
    }
    struct ucred creds{};
    if (!FaultCommonUtil::GetUcredByPeerCred(creds, connectionFd) ||
            creds.uid != static_cast<uint32_t>(requestData.uid)) {
        DFXLOGW("Failed to check request credential request uid:%{public}d: cred uid:%{public}d fd:%{public}d",
                requestData.uid, creds.uid, connectionFd);
        return false;
    }
    return true;
}

int32_t ExceptionReportService::OnRequest(const std::string& socketName, int32_t connectionFd,
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
#ifdef FAULTLOGGERD_TEST
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
#endif
    return ResponseCode::REQUEST_SUCCESS;
}

void StatsService::RemoveTimeoutDumpStats()
{
    constexpr uint64_t timeout = 10000; // 10s
    uint64_t now = GetTimeMilliSeconds();
    stats_.remove_if([&now, &timeout](const auto& stats) {
        return (now > stats.signalTime && now - stats.signalTime > timeout) ||
            now <= stats.signalTime;
    });
}

void StatsService::ReportDumpStats(const DumpStats& stat)
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

std::string StatsService::GetElfName(const FaultLoggerdStatsRequest& request)
{
    if (strlen(request.callerElf) > NAME_BUF_LEN) {
        return "";
    }
    return StringPrintf("%s(%p)", request.callerElf, reinterpret_cast<void*>(request.offset));
}

int32_t StatsService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const FaultLoggerdStatsRequest& requestData)
{
    constexpr int32_t delayTime = 7; // allow 10s for processdump report, 3s for dumpcatch timeout and 7s for delay
    DFXLOGI("%{public}s :: %{public}s: HandleDumpStats", FAULTLOGGERD_SERVICE_TAG, __func__);
    auto iter = std::find_if(stats_.begin(), stats_.end(), [&requestData](const DumpStats& dumpStats) {
        return dumpStats.pid == requestData.pid;
    });
    if (requestData.type == PROCESS_DUMP && iter == stats_.end()) {
        auto& stats = stats_.emplace_back();
        stats.pid = requestData.pid;
        stats.signalTime = requestData.signalTime;
        stats.processdumpStartTime = requestData.processdumpStartTime;
        stats.processdumpFinishTime = requestData.processdumpFinishTime;
        stats.targetProcessName = requestData.targetProcess;
    } else {
        auto task = [requestData, this] {
            auto iter = std::find_if(stats_.begin(), stats_.end(), [&requestData](const DumpStats& dumpStats) {
                return dumpStats.pid == requestData.pid;
            });
            if (requestData.type == DUMP_CATCHER && iter != stats_.end()) {
                iter->requestTime = requestData.requestTime;
                iter->dumpCatcherFinishTime = requestData.dumpCatcherFinishTime;
                iter->callerElfName = GetElfName(requestData);
                iter->callerProcessName = requestData.callerProcess;
                iter->result = requestData.result;
                iter->summary = requestData.summary;
                ReportDumpStats(*iter);
                stats_.erase(iter);
            } else if (requestData.type == DUMP_CATCHER) {
                DumpStats stats;
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
        };
        StartDelayTask(task, delayTime);
    }
    RemoveTimeoutDumpStats();
#ifdef FAULTLOGGERD_TEST
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
#endif
    return ResponseCode::REQUEST_SUCCESS;
}

void StatsService::StartDelayTask(std::function<void()> workFunc, int32_t delayTime)
{
    auto delayTask = DelayTask::CreateInstance(workFunc, delayTime);
    FaultLoggerDaemon::GetEpollManager(EpollManagerType::MAIN_SERVER).AddListener(std::move(delayTask));
}
#endif

int32_t FileDesService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const FaultLoggerdRequest& requestData)
{
    DFX_TRACE_SCOPED("FileDesServiceOnRequest");
    if (!Filter(socketName, connectionFd, requestData)) {
        return ResponseCode::REQUEST_REJECT;
    }
    SmartFd fileFd(TempFileManager::CreateFileDescriptor(requestData.type, requestData.pid,
        requestData.tid, requestData.time));
    if (!fileFd) {
        return ResponseCode::ABNORMAL_SERVICE;
    }
#ifndef is_ohos_lite
    TempFileManager::RecordFileCreation(requestData.type, requestData.pid);
#endif
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
    int fd = fileFd.GetFd();
    SendFileDescriptorToSocket(connectionFd, &fd, 1);
    return responseData;
}

bool FileDesService::Filter(const std::string& socketName, int32_t connectionFd,
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
int32_t SdkDumpService::Filter(const std::string& socketName, const SdkDumpRequestData& requestData, uint32_t uid)
{
    if (requestData.pid <= 0 || socketName != SERVER_SDKDUMP_SOCKET_NAME || !CheckCallerUID(uid)) {
        DFXLOGE("%{public}s :: HandleSdkDumpRequest :: pid(%{public}d) or socketName(%{public}s) fail.",
            FAULTLOGGERD_SERVICE_TAG, requestData.pid, socketName.c_str());
        return ResponseCode::REQUEST_REJECT;
    }
    if (TempFileManager::CheckCrashFileRecord(requestData.pid)) {
        DFXLOGW("%{public}s :: pid(%{public}d) has been crashed, break.",
                FAULTLOGGERD_SERVICE_TAG, requestData.pid);
        return ResponseCode::SDK_PROCESS_CRASHED;
    }
    if (FaultLoggerPipePair::CheckSdkDumpRecord(requestData.pid, requestData.time)) {
        DFXLOGE("%{public}s :: pid(%{public}d) is dumping, break.", FAULTLOGGERD_SERVICE_TAG, requestData.pid);
        return ResponseCode::SDK_DUMP_REPEAT;
    }
    return ResponseCode::REQUEST_SUCCESS;
}

int32_t SdkDumpService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const SdkDumpRequestData& requestData)
{
    DFX_TRACE_SCOPED("SdkDumpServiceOnRequest");
    DFXLOGI("Receive dump request for pid:%{public}d tid:%{public}d.", requestData.pid, requestData.tid);
    struct ucred creds;
    if (!FaultCommonUtil::GetUcredByPeerCred(creds, connectionFd)) {
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
    auto& faultLoggerPipe = FaultLoggerPipePair::CreateSdkDumpPipePair(requestData.pid, requestData.time);

    if (auto ret = FaultCommonUtil::SendSignalToProcess(requestData.pid, si); ret != ResponseCode::REQUEST_SUCCESS) {
        FaultLoggerPipePair::DelSdkDumpPipePair(requestData.pid);
        return ret;
    }

    int32_t fds[PIPE_NUM_SZ] = {
        faultLoggerPipe.GetPipeFd(PipeFdUsage::BUFFER_FD, FaultLoggerPipeType::PIPE_FD_READ),
        faultLoggerPipe.GetPipeFd(PipeFdUsage::RESULT_FD, FaultLoggerPipeType::PIPE_FD_READ)
    };
    if (fds[PIPE_BUF_INDEX] < 0 || fds[PIPE_RES_INDEX] < 0) {
        return ResponseCode::ABNORMAL_SERVICE;
    }
    int32_t res = ResponseCode::REQUEST_SUCCESS;
    SendMsgToSocket(connectionFd, &res, sizeof(res));
    SendFileDescriptorToSocket(connectionFd, fds, PIPE_NUM_SZ);
    return res;
}

bool PipeService::Filter(const std::string &socketName, int32_t connectionFd, const PipFdRequestData &requestData)
{
    if (requestData.pipeType > FaultLoggerPipeType::PIPE_FD_DELETE ||
        requestData.pipeType < FaultLoggerPipeType::PIPE_FD_READ) {
        return false;
    }
    if (socketName == SERVER_CRASH_SOCKET_NAME) {
        return true;
    }
    return CheckRequestCredential(connectionFd, requestData.pid);
}

int32_t PipeService::OnRequest(const std::string& socketName, int32_t connectionFd, const PipFdRequestData& requestData)
{
    DFX_TRACE_SCOPED("PipeServiceOnRequest");
    if (!Filter(socketName, connectionFd, requestData)) {
        return ResponseCode::REQUEST_REJECT;
    }
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    if (requestData.pipeType == FaultLoggerPipeType::PIPE_FD_DELETE) {
        FaultLoggerPipePair::DelSdkDumpPipePair(requestData.pid);
        SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
        return responseData;
    }
    FaultLoggerPipePair* faultLoggerPipe = FaultLoggerPipePair::GetSdkDumpPipePair(requestData.pid);
    if (faultLoggerPipe == nullptr) {
        DFXLOGE("%{public}s :: cannot find pipe fd for pid(%{public}d).", FAULTLOGGERD_SERVICE_TAG, requestData.pid);
        return ResponseCode::ABNORMAL_SERVICE;
    }
    int32_t fds[PIPE_NUM_SZ] = {
        faultLoggerPipe->GetPipeFd(PipeFdUsage::BUFFER_FD, FaultLoggerPipeType::PIPE_FD_WRITE),
        faultLoggerPipe->GetPipeFd(PipeFdUsage::RESULT_FD, FaultLoggerPipeType::PIPE_FD_WRITE)
    };
    if (fds[PIPE_BUF_INDEX] < 0 || fds[PIPE_RES_INDEX] < 0) {
        DFXLOGE("%{public}s :: failed to get fds for pipeType(%{public}d).", FAULTLOGGERD_SERVICE_TAG,
            requestData.pipeType);
        return ResponseCode::ABNORMAL_SERVICE;
    }
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
    SendFileDescriptorToSocket(connectionFd, fds, PIPE_NUM_SZ);
    return responseData;
}

bool LitePerfPipeService::Filter(const std::string &socketName, int32_t connectionFd,
    const PipFdRequestData &requestData, int& uid)
{
    if (requestData.pipeType > FaultLoggerPipeType::PIPE_FD_DELETE ||
        requestData.pipeType < FaultLoggerPipeType::PIPE_FD_READ) {
        return false;
    }

    struct ucred creds{};
    if (!FaultCommonUtil::GetUcredByPeerCred(creds, connectionFd)) {
        return false;
    }
    if (creds.pid != requestData.pid) {
        DFXLOGW("Failed to check request credential request:%{public}d, cred:%{public}d, fd:%{public}d",
            requestData.pid, creds.pid, connectionFd);
        return false;
    }
    uid = creds.uid;
    return true;
}

int32_t LitePerfPipeService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const PipFdRequestData& requestData)
{
    DFX_TRACE_SCOPED("LitePerfPipeServiceOnRequest");
    int uid;
    if (!Filter(socketName, connectionFd, requestData, uid)) {
        return ResponseCode::REQUEST_REJECT;
    }
    int32_t responseData = ResponseCode::REQUEST_SUCCESS;
    if (requestData.pipeType == FaultLoggerPipeType::PIPE_FD_DELETE) {
        LitePerfPipePair::DelPipePair(uid);
        SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
        return responseData;
    }

    int32_t fds[PIPE_NUM_SZ] = {0};
    if (requestData.pipeType == FaultLoggerPipeType::PIPE_FD_READ) {
        if (LitePerfPipePair::CheckDumpRecord(uid)) {
            DFXLOGE("%{public}s :: uid(%{public}d) is dumping.", FAULTLOGGERD_SERVICE_TAG, uid);
            return ResponseCode::SDK_DUMP_REPEAT;
        }
        auto& pipePair = LitePerfPipePair::CreatePipePair(uid);
        fds[PIPE_BUF_INDEX] = pipePair.GetPipeFd(PipeFdUsage::BUFFER_FD, FaultLoggerPipeType::PIPE_FD_READ);
        fds[PIPE_RES_INDEX] = pipePair.GetPipeFd(PipeFdUsage::RESULT_FD, FaultLoggerPipeType::PIPE_FD_READ);
    } else if (requestData.pipeType == FaultLoggerPipeType::PIPE_FD_WRITE) {
        LitePerfPipePair* pipePair = LitePerfPipePair::GetPipePair(uid);
        if (pipePair == nullptr) {
            DFXLOGE("%{public}s :: cannot find pipe fd for pid(%{public}d).",
                FAULTLOGGERD_SERVICE_TAG, requestData.pid);
            return ResponseCode::ABNORMAL_SERVICE;
        }
        fds[PIPE_BUF_INDEX] = pipePair->GetPipeFd(PipeFdUsage::BUFFER_FD, FaultLoggerPipeType::PIPE_FD_WRITE);
        fds[PIPE_RES_INDEX] = pipePair->GetPipeFd(PipeFdUsage::RESULT_FD, FaultLoggerPipeType::PIPE_FD_WRITE);
    }
    if (fds[PIPE_BUF_INDEX] < 0 || fds[PIPE_RES_INDEX] < 0) {
        DFXLOGE("%{public}s :: failed to get fds for pipeType(%{public}d).", FAULTLOGGERD_SERVICE_TAG,
            requestData.pipeType);
        return ResponseCode::ABNORMAL_SERVICE;
    }
    SendMsgToSocket(connectionFd, &responseData, sizeof(responseData));
    SendFileDescriptorToSocket(connectionFd, fds, PIPE_NUM_SZ);
    return responseData;
}
#endif
}
}