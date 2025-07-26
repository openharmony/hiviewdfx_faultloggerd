/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#include "process_dumper.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <pthread.h>
#include <securec.h>
#include <string>
#include <syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include "reporter.h"
#include "crash_exception.h"
#include "process_dump_config.h"
#include "dump_info_factory.h"
#include "dump_utils.h"
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_dump_res.h"

#include "dfx_ptrace.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif
#include "dfx_buffer_writer.h"
#include "dump_info_json_formatter.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "dfx_trace.h"
#include "directory_ex.h"
#include "elapsed_time.h"
#include "faultloggerd_client.h"
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif
#include "info/fatal_message.h"
#include "procinfo.h"
#include "reporter.h"
#include "unwinder_config.h"
#ifndef is_ohos_lite
#include "hitrace/hitracechainc.h"
#include "parameters.h"
#endif // !is_ohos_lite
#include "info/fatal_message.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxProcessDump"
const char *const BLOCK_CRASH_PROCESS = "faultloggerd.priv.block_crash_process.enabled";
MAYBE_UNUSED const char *const MIXSTACK_ENABLE = "faultloggerd.priv.mixstack.enabled";
const char * const OTHER_THREAD_DUMP_INFO = "OtherThreadDumpInfo";

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
void SigAlarmCallBack()
{
    ProcessDumper::GetInstance().ReportSigDumpStats();
}
#endif

static bool IsBlockCrashProcess()
{
#ifndef is_ohos_lite
    static bool isBlockCrash = OHOS::system::GetParameter(BLOCK_CRASH_PROCESS, "false") == "true";
    return isBlockCrash;
#else
    return false;
#endif
}

void WaitForFork(unsigned long pid, unsigned long& childPid)
{
    DFXLOGI("start wait fork event happen");
    int waitStatus = 0;
    waitpid(pid, &waitStatus, 0); // wait fork event
    DFXLOGI("wait for fork status %{public}d", waitStatus);
    if (static_cast<unsigned int>(waitStatus) >> 8 == (SIGTRAP | (PTRACE_EVENT_FORK << 8))) { // 8 : get fork event
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &childPid);
        DFXLOGI("next child pid %{public}lu", childPid);
        waitpid(childPid, &waitStatus, 0); // wait child stop event
    }
}

void GetVmProcessRealPid(const ProcessDumpRequest& request, unsigned long vmPid, int& realPid)
{
    int waitStatus = 0;
    ptrace(PTRACE_SETOPTIONS, vmPid, NULL, PTRACE_O_TRACEEXIT); // block son exit
    ptrace(PTRACE_CONT, vmPid, NULL, NULL);

    DFXLOGI("start wait exit event happen");
    waitpid(vmPid, &waitStatus, 0); // wait exit stop
    long data = ptrace(PTRACE_PEEKDATA, vmPid, reinterpret_cast<void *>(request.vmProcRealPidAddr), nullptr);
    if (data < 0) {
        DFXLOGI("ptrace peek data error %{public}lu %{public}d", vmPid, errno);
    }
    realPid = static_cast<int>(data);
}

void ReadPids(const ProcessDumpRequest& request, int& realPid, int& vmPid, DfxProcess process)
{
    unsigned long childPid = 0;
    DFX_TRACE_SCOPED("ReadPids");
    WaitForFork(request.tid, childPid);
    ptrace(PTRACE_CONT, childPid, NULL, NULL);

    // freeze detach after fork child to fork vm process
    if (request.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH ||
#if defined(__aarch64__)
        CoreDumpService::IsCoredumpSignal(request)
#else
        false
#endif
    ) {
        process.Detach();
        DFXLOGI("ptrace detach all tids");
    }

    unsigned long sonPid = 0;
    WaitForFork(childPid, sonPid);
    vmPid = static_cast<int>(sonPid);

    ptrace(PTRACE_DETACH, childPid, 0, 0);
    GetVmProcessRealPid(request, sonPid, realPid);

    DFXLOGI("procecdump get real pid is %{public}d vm pid is %{public}d", realPid, vmPid);
}

void NotifyOperateResult(ProcessDumpRequest& request, int result)
{
    if (request.childPipeFd[0] != -1) {
        close(request.childPipeFd[0]);
        request.childPipeFd[0] = -1;
    }
    if (OHOS_TEMP_FAILURE_RETRY(write(request.childPipeFd[1], &result, sizeof(result))) < 0) {
        DFXLOGE("write to child process fail %{public}d", errno);
    }
}

void BlockCrashProcExit(const ProcessDumpRequest& request)
{
    if (!IsBlockCrashProcess()) {
        return;
    }
    DFXLOGI("start block crash process pid %{public}d nspid %{public}d", request.pid, request.nsPid);
    if (ptrace(PTRACE_POKEDATA, request.nsPid, reinterpret_cast<void*>(request.blockCrashExitAddr),
        CRASH_BLOCK_EXIT_FLAG) < 0) {
        DFXLOGE("pok block falg to nsPid %{public}d fail %{public}s", request.nsPid, strerror(errno));
    }
}
}

ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper ins;
    return ins;
}

void ProcessDumper::Dump()
{
    startTime_ = GetTimeMillisec();
    int resDump = DumpProcess();
    FormatJsonInfoIfNeed();
    WriteDumpResIfNeed(resDump);
    finishTime_ = GetTimeMillisec();

    ReportSigDumpStats();
    DfxBufferWriter::GetInstance().PrintBriefDumpInfo();
    DfxBufferWriter::GetInstance().Finish();
    DFXLOGI("Finish dump stacktrace for %{public}s(%{public}d:%{public}d).",
        request_.processName, request_.pid, request_.tid);
    // if the process no longer exists before dumping, do not report sysevent
    if (process_ != nullptr && resDump != DumpErrorCode::DUMP_ENOMAP &&
        resDump != DumpErrorCode::DUMP_EREADPID) {
        SysEventReporter reporter(request_.type);
        reporter.Report(*process_, request_);
    }

    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        DumpUtils::InfoCrashUnwindResult(request_, resDump == DumpErrorCode::DUMP_ESUCCESS);
        BlockCrashProcExit(request_);
    }
    if (process_ != nullptr) {
        process_->Detach();
    }
}

int ProcessDumper::DumpProcess()
{
    DFX_TRACE_SCOPED("DumpProcess");
    int dumpRes = ReadRequestAndCheck();
    if (dumpRes != DumpErrorCode::DUMP_ESUCCESS) {
        return dumpRes;
    }
#ifndef is_ohos_lite
    if (sizeof(HiTraceIdStruct) == sizeof(DumpHiTraceIdStruct)) {
        HiTraceIdStruct* hitraceIdPtr = reinterpret_cast<HiTraceIdStruct*>(&request_.hitraceId);
        hitraceIdPtr->flags |= HITRACE_FLAG_INCLUDE_ASYNC;
        HiTraceChainSetId(hitraceIdPtr);
    } else {
        DFXLOGE("No invalid hitraceId struct size!");
    }
#endif
    DFXLOGI("Processdump SigVal(%{public}d), TargetPid(%{public}d:%{public}d), TargetTid(%{public}d), " \
        "threadname(%{public}s).",
        request_.siginfo.si_value.sival_int, request_.pid, request_.nsPid, request_.tid, request_.threadName);
    SetProcessdumpTimeout(request_.siginfo);
    UpdateConfigByRequest();
    if (!InitDfxProcess()) {
        DFXLOGE("Failed to init crash process info.");
        dumpRes = DumpErrorCode::DUMP_EATTACH;
        return dumpRes;
    }
    if (!InitUnwinder(dumpRes)) {
        return dumpRes;
    }
    if (!InitBufferWriter()) {
        DFXLOGE("Failed to init buffer writer.");
        dumpRes = DumpErrorCode::DUMP_EGETFD;
    }
    PrintDumpInfo(dumpRes);
    UnwindWriteJit();
    DfxPtrace::Detach(process_->GetVmPid());
    return dumpRes;
}

void ProcessDumper::SetProcessdumpTimeout(siginfo_t &si)
{
    if (si.si_signo != SIGDUMP) {
        return;
    }

    int tid;
    ParseSiValue(si, expectedDumpFinishTime_, tid);

    if (tid == 0) {
        DFXLOGI("reset, prevent incorrect reading sival_int for tid");
        si.si_value.sival_int = 0;
    }

    if (expectedDumpFinishTime_ == 0) {
        DFXLOGI("end time is zero, not set new alarm");
        return;
    }

    uint64_t curTime = GetAbsTimeMilliSeconds();
    if (curTime >= expectedDumpFinishTime_) {
        DFXLOGI("now has timeout, processdump exit");
        ReportSigDumpStats();
#ifndef CLANG_COVERAGE
        _exit(0);
#endif
    }
    uint64_t diffTime = expectedDumpFinishTime_ - curTime;

    DFXLOGI("processdump remain time%{public}" PRIu64 "ms", diffTime);
    if (diffTime > PROCESSDUMP_TIMEOUT * NUMBER_ONE_THOUSAND) {
        DFXLOGE("dump remain time is invalid, not set timer");
        return;
    }

    struct itimerval timer{};
    timer.it_value.tv_sec = static_cast<int64_t>(diffTime / NUMBER_ONE_THOUSAND);
    timer.it_value.tv_usec = static_cast<int64_t>(diffTime * NUMBER_ONE_THOUSAND % NUMBER_ONE_MILLION);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
        DFXLOGE("start processdump timer fail %{public}d", errno);
    }
}

void ProcessDumper::FormatJsonInfoIfNeed()
{
    if (!isJsonDump_ && request_.type != ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        return;
    }
    if (process_ == nullptr) {
        return;
    }
    std::string jsonInfo;
    DumpInfoJsonFormatter stackInfoFormatter;
    stackInfoFormatter.GetJsonFormatInfo(request_, *process_, jsonInfo);
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        process_->SetCrashInfoJson(jsonInfo);
    } else if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        DfxBufferWriter::GetInstance().WriteMsg(jsonInfo);
    }
    DFXLOGI("Finish GetJsonFormatInfo len %{public}" PRIuPTR "", jsonInfo.length());
}

void ProcessDumper::WriteDumpResIfNeed(int32_t resDump)
{
    DFXLOGI("dump result: %{public}s", DfxDumpRes::ToString(resDump).c_str());
    if (request_.siginfo.si_signo != SIGDUMP) {
        return;
    }
    if (!DfxBufferWriter::GetInstance().WriteDumpRes(resDump)) { // write dump res, retry request_ pipefd
        int pipeWriteFd[] = { -1, -1 };
        if (RequestPipeFd(request_.pid, FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd) == -1) {
            DFXLOGE("%{public}s request_ pipe failed, err:%{public}d", __func__, errno);
            return;
        }
        SmartFd bufFd(pipeWriteFd[PIPE_BUF_INDEX]);
        SmartFd resFd(pipeWriteFd[PIPE_RES_INDEX]);
        DfxBufferWriter::GetInstance().SetWriteResFd(std::move(resFd));
        DfxBufferWriter::GetInstance().WriteDumpRes(resDump);
    }
}

int32_t ProcessDumper::ReadRequestAndCheck()
{
    DFX_TRACE_SCOPED("ReadRequestAndCheck");
    ElapsedTime counter("ReadRequestAndCheck", 20); // 20 : limit cost time 20 ms
    ssize_t readCount = OHOS_TEMP_FAILURE_RETRY(read(STDIN_FILENO, &request_, sizeof(ProcessDumpRequest)));
    request_.threadName[NAME_BUF_LEN - 1] = '\0';
    request_.processName[NAME_BUF_LEN - 1] = '\0';
    request_.msg.body[MAX_FATAL_MSG_SIZE - 1] = '\0';
    request_.appRunningId[MAX_APP_RUNNING_UNIQUE_ID_LEN - 1] = '\0';
    auto processName = std::string(request_.processName);
    SetCrashProcInfo(request_.type, processName, request_.pid, request_.uid);
    if (readCount != static_cast<long>(sizeof(ProcessDumpRequest))) {
        DFXLOGE("Failed to read DumpRequest(%{public}d), readCount(%{public}zd).", errno, readCount);
        ReportCrashException(CrashExceptionCode::CRASH_DUMP_EREADREQ);
        return DumpErrorCode::DUMP_EREADREQUEST;
    }
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
    DFX_SetSigAlarmCallBack(SigAlarmCallBack);
#endif
    return DumpErrorCode::DUMP_ESUCCESS;
}


void ProcessDumper::UpdateConfigByRequest()
{
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        if (request_.siginfo.si_value.sival_int != 0) { // if not equal 0, need not dump other thread info
            DFXLOGI("updateconfig.");
            auto dumpInfoComponent = FindDumpInfoByType(request_.type);
            dumpInfoComponent.erase(std::remove(dumpInfoComponent.begin(), dumpInfoComponent.end(),
                OTHER_THREAD_DUMP_INFO), dumpInfoComponent.end());
            auto config = ProcessDumpConfig::GetInstance().GetConfig();
            config.dumpInfo[request_.type] = dumpInfoComponent;
            for (const auto& info : dumpInfoComponent) {
                DFXLOGI("component%{public}s.", info.c_str());
            }
            ProcessDumpConfig::GetInstance().SetConfig(std::move(config));
        }
        isJsonDump_ =  request_.siginfo.si_code == DUMP_TYPE_REMOTE_JSON;
    }
}

void ProcessDumper::UnwindWriteJit()
{
    if (request_.type != ProcessDumpType::DUMP_TYPE_CPP_CRASH || !unwinder_) {
        return;
    }

    const auto& jitCache = unwinder_->GetJitCache();
    if (jitCache.empty()) {
        return;
    }
    struct FaultLoggerdRequest jitRequest{
        .pid = request_.pid,
        .type = FaultLoggerType::JIT_CODE_LOG,
        .tid = request_.tid,
        .time = request_.timeStamp
    };
    SmartFd fd(RequestFileDescriptorEx(&jitRequest));
    if (!fd) {
        DFXLOGE("request_ jitlog fd failed.");
        return;
    }

    if (unwinder_->ArkWriteJitCodeToFile(fd.GetFd()) < 0) {
        DFXLOGE("jit code write file failed.");
    }
}

void ProcessDumper::PrintDumpInfo(int& dumpRes)
{
    DFX_TRACE_SCOPED("PrintDumpInfo");
    if (process_ == nullptr || unwinder_ == nullptr) {
        return;
    }
    auto dumpInfoComponent = FindDumpInfoByType(request_.type);
    if (dumpInfoComponent.empty()) {
        return;
    }
    // Create objects using reflection
    auto threadDumpInfo = DumpInfoFactory::GetInstance().CreateObject(dumpInfoComponent[0]);
    auto prevDumpInfo = threadDumpInfo;
    auto dumpInfo = threadDumpInfo;
    for (size_t index = 1; index < dumpInfoComponent.size(); index++) {
        auto tempDumpInfo = DumpInfoFactory::GetInstance().CreateObject(dumpInfoComponent[index]);
        if (tempDumpInfo == nullptr) {
            DFXLOGE("Failed to crreate object%{public}s.", dumpInfoComponent[index].c_str());
            continue;
        }
        dumpInfo = tempDumpInfo;
        dumpInfo->SetDumpInfo(prevDumpInfo);
        if (dumpInfoComponent[index] == OTHER_THREAD_DUMP_INFO) {
            threadDumpInfo = dumpInfo;
        }
        prevDumpInfo = dumpInfo;
    }
    if (threadDumpInfo != nullptr) {
        int unwindSuccessCnt = threadDumpInfo->UnwindStack(*process_, *unwinder_);
        DFXLOGI("unwind success thread count(%{public}d)", unwindSuccessCnt);
        if (unwindSuccessCnt > 0) {
            dumpRes = ParseSymbols(threadDumpInfo);
        } else {
            dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
            DFXLOGE("Failed to unwind process.");
        }
    }

    if (dumpInfo != nullptr && !isJsonDump_) { // isJsonDump_ will print after format json
        dumpInfo->Print(*process_, request_, *unwinder_);
    }
}

int ProcessDumper::ParseSymbols(std::shared_ptr<DumpInfo> threadDumpInfo)
{
    uint64_t curTime = GetAbsTimeMilliSeconds();
    uint32_t lessRemainTimeMs =
        static_cast<uint32_t>(ProcessDumpConfig::GetInstance().GetConfig().reservedParseSymbolTime);
    int dumpRes = 0;
    if (request_.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH || expectedDumpFinishTime_ == 0) {
        threadDumpInfo->Symbolize(*process_, *unwinder_);
    } else if (expectedDumpFinishTime_ > curTime && expectedDumpFinishTime_ - curTime > lessRemainTimeMs) {
        parseSymbolTask_ = std::async(std::launch::async, [threadDumpInfo, this]() {
            DFX_TRACE_SCOPED("parse symbol task");
            threadDumpInfo->Symbolize(*process_, *unwinder_);
        });
        uint64_t waitTime = expectedDumpFinishTime_ - curTime - lessRemainTimeMs;
        if (parseSymbolTask_.wait_for(std::chrono::milliseconds(waitTime)) != std::future_status::ready) {
            DFXLOGW("Parse symbol timeout");
            dumpRes = DumpErrorCode::DUMP_ESYMBOL_PARSE_TIMEOUT;
        }
    } else {
        DFXLOGW("do not parse symbol, remain %{public}" PRId64 "ms", expectedDumpFinishTime_ - curTime);
        dumpRes = DumpErrorCode::DUMP_ESYMBOL_NO_PARSE;
    }
    finishParseSymbolTime_ = GetTimeMillisec();
    return dumpRes;
}

bool ProcessDumper::InitUnwinder(int &dumpRes)
{
    DFX_TRACE_SCOPED("InitUnwinder");
    if (process_ == nullptr) {
        DFXLOGE("Failed to read real pid!");
        return false;
    }
    pid_t realPid = 0;
    pid_t vmPid = 0;
    ReadPids(request_, realPid, vmPid, *process_);
    if (realPid <= 0 || vmPid <= 0) {
        ReportCrashException(CrashExceptionCode::CRASH_DUMP_EREADPID);
        DFXLOGE("Failed to read real pid!");
        dumpRes = DumpErrorCode::DUMP_EREADPID;
        return false;
    }
#if defined(__aarch64__)
    if (coreDumpService_) {
        coreDumpService_->StartSecondStageDump(vmPid, request_);
    }
#endif
    process_->SetVmPid(vmPid);
#if defined(PROCESSDUMP_MINIDEBUGINFO)
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
    unwinder_ = std::make_shared<Unwinder>(realPid, vmPid, request_.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH);
    unwinder_->EnableParseNativeSymbol(false);
    if (unwinder_->GetMaps() == nullptr) {
        ReportCrashException(CrashExceptionCode::CRASH_LOG_EMAPLOS);
        DFXLOGE("Mapinfo of crashed process is not exist!");
        dumpRes = DumpErrorCode::DUMP_ENOMAP;
        return false;
    }
    return true;
}

std::vector<std::string> ProcessDumper::FindDumpInfoByType(const ProcessDumpType& dumpType)
{
    std::vector<std::string> dumpInfoComponent;
    auto dumpInfo = ProcessDumpConfig::GetInstance().GetConfig().dumpInfo;
    if (dumpInfo.find(dumpType) != dumpInfo.end()) {
        dumpInfoComponent = dumpInfo.find(dumpType)->second;
    }
    return dumpInfoComponent;
}

bool ProcessDumper::InitDfxProcess()
{
    DFX_TRACE_SCOPED("InitDfxProcess");
    if (request_.pid <= 0) {
        return false;
    }
    process_ = std::make_shared<DfxProcess>();
    process_->InitProcessInfo(request_.pid, request_.nsPid, request_.uid, std::string(request_.processName));
    if (!process_->InitKeyThread(request_)) {
        NotifyOperateResult(request_, OPE_FAIL);
        return false;
    }
#if defined(__aarch64__)
    if (CoreDumpService::IsCoredumpAllowed(request_)) {
        coreDumpService_ = std::unique_ptr<CoreDumpService>(new CoreDumpService());
    }
    if (coreDumpService_) {
        coreDumpService_->GetKeyThreadData(request_);
    }
#endif
    DFXLOGI("Init key thread successfully.");
    NotifyOperateResult(request_, OPE_SUCCESS);
    ptrace(PTRACE_SETOPTIONS, request_.tid, NULL, PTRACE_O_TRACEFORK);
    ptrace(PTRACE_CONT, request_.tid, 0, 0);

    auto dumpInfoComp = FindDumpInfoByType(request_.type);
    if (std::find(dumpInfoComp.begin(), dumpInfoComp.end(), OTHER_THREAD_DUMP_INFO) != dumpInfoComp.end()) {
        process_->InitOtherThreads(request_.tid);
    }
    DFXLOGI("Finish create all thread.");
#if defined(__aarch64__)
    if (coreDumpService_) {
        coreDumpService_->StartFirstStageDump(request_);
    }
#endif
    return true;
}

int ProcessDumper::GeFaultloggerdRequestType()
{
    switch (request_.siginfo.si_signo) {
        case SIGLEAK_STACK:
            switch (request_.siginfo.si_code) {
                case SIGLEAK_STACK_FDSAN:
                    FALLTHROUGH_INTENDED;
                case SIGLEAK_STACK_JEMALLOC:
                    FALLTHROUGH_INTENDED;
                case SIGLEAK_STACK_BADFD:
                    return FaultLoggerType::CPP_STACKTRACE;
                default:
                    return FaultLoggerType::LEAK_STACKTRACE;
            }
        case SIGDUMP:
            return FaultLoggerType::CPP_STACKTRACE;
        default:
            return FaultLoggerType::CPP_CRASH;
    }
}

bool ProcessDumper::InitBufferWriter()
{
    DFX_TRACE_SCOPED("InitBufferWriter");
    SmartFd bufferFd;
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        int pipeWriteFd[] = { -1, -1 };
        if (RequestPipeFd(request_.pid, FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd) == 0) {
            bufferFd = SmartFd{pipeWriteFd[PIPE_BUF_INDEX]};
            DfxBufferWriter::GetInstance().SetWriteResFd(SmartFd{pipeWriteFd[PIPE_RES_INDEX]});
        }
    } else {
        struct FaultLoggerdRequest faultloggerdRequest{
            .pid = request_.pid,
            .type = GeFaultloggerdRequestType(),
            .tid = request_.tid,
            .time = request_.timeStamp
        };
        bufferFd = SmartFd {RequestFileDescriptorEx(&faultloggerdRequest)};
        if (!bufferFd) {
            DFXLOGW("Failed to request_ fd from faultloggerd.");
            ReportCrashException(CrashExceptionCode::CRASH_DUMP_EWRITEFD);
            bufferFd = SmartFd {CreateFileForCrash(request_.pid, request_.timeStamp)};
        }
    }

    bool rst{bufferFd};
    DfxBufferWriter::GetInstance().SetWriteBufFd(std::move(bufferFd));
    return rst;
}

int32_t ProcessDumper::CreateFileForCrash(int32_t pid, uint64_t time) const
{
    const std::string dirPath = "/log/crash";
    const std::string logFileType = "cppcrash";
    const int32_t logcrashFileProp = 0644; // 0640:-rw-r--r--
    if (access(dirPath.c_str(), F_OK) != 0) {
        DFXLOGE("%{public}s is not exist.", dirPath.c_str());
        return INVALID_FD;
    }
    RemoveFileIfNeed(dirPath);
    std::string logPath = dirPath + "/" + logFileType + "-" + std::to_string(pid) + "-" + std::to_string(time);
    int32_t fd = OHOS_TEMP_FAILURE_RETRY(open(logPath.c_str(), O_RDWR | O_CREAT, logcrashFileProp));
    if (fd == INVALID_FD) {
        DFXLOGE("create %{public}s failed, errno=%{public}d", logPath.c_str(), errno);
    } else {
        DFXLOGI("create crash path %{public}s succ.", logPath.c_str());
    }
    return fd;
}

void ProcessDumper::RemoveFileIfNeed(const std::string& dirPath) const
{
    const int maxFileCount = 5;
    std::vector<std::string> files;
    OHOS::GetDirFiles(dirPath, files);
    if (files.size() < maxFileCount) {
        return;
    }

    std::sort(files.begin(), files.end(),
        [](const std::string& lhs, const std::string& rhs) {
        auto lhsSplitPos = lhs.find_last_of("-");
        auto rhsSplitPos = rhs.find_last_of("-");
        if (lhsSplitPos == std::string::npos || rhsSplitPos == std::string::npos) {
            return lhs.compare(rhs) < 0;
        }
        return lhs.substr(lhsSplitPos).compare(rhs.substr(rhsSplitPos)) < 0;
    });

    int deleteNum = static_cast<int>(files.size()) - (maxFileCount - 1);
    for (int index = 0; index < deleteNum; index++) {
        DFXLOGI("Now we delete file(%{public}s) due to exceed file max count.", files[index].c_str());
        OHOS::RemoveFile(files[index]);
    }
}

void ProcessDumper::ReportSigDumpStats()
{
    static std::atomic<bool> hasReportSigDumpStats(false);
    if (request_.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH || hasReportSigDumpStats == true) {
        return;
    }
    std::vector<uint8_t> buf(sizeof(struct FaultLoggerdStatsRequest), 0);
    auto stat = reinterpret_cast<struct FaultLoggerdStatsRequest*>(buf.data());
    stat->type = PROCESS_DUMP;
    stat->pid = request_.pid;
    stat->signalTime = request_.timeStamp;
    stat->processdumpStartTime = startTime_;
    stat->processdumpFinishTime = finishTime_ == 0 ? GetTimeMillisec() : finishTime_;
    stat->writeDumpInfoCost = finishParseSymbolTime_ > 0 ? stat->processdumpFinishTime - finishParseSymbolTime_ : 0;
    if (memcpy_s(stat->targetProcess, sizeof(stat->targetProcess),
        request_.processName, sizeof(request_.processName)) != 0) {
        DFXLOGE("Failed to copy target processName (%{public}d)", errno);
        return;
    }
    ReportDumpStats(stat);
    hasReportSigDumpStats.store(true);
}
} // namespace HiviewDFX
} // namespace OHOS
