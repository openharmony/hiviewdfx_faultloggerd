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

#include <fstream>
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
#include <ucontext.h>
#include <unistd.h>

#include "reporter.h"
#include "crash_exception.h"
#include "process_dump_config.h"
#include "dump_info_factory.h"
#include "dump_utils.h"
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_log.h"
#include "dfx_ptrace.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "key_thread_dump_info.h"
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
#include "procinfo.h"
#include "proc_util.h"
#include "reporter.h"
#include "unwinder_config.h"
#ifndef is_ohos_lite
#include "hitrace/hitracechainc.h"
#endif // !is_ohos_lite
#if defined(__aarch64__) && !defined(is_ohos_lite)
#include "coredump_controller.h"
#endif
namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxProcessDump"
const char * const OTHER_THREAD_DUMP_INFO = "OtherThreadDumpInfo";
MAYBE_UNUSED constexpr int PROCESSDUMP_REMAIN_TIME = 2500;
constexpr size_t TIME_BUFFER_SIZE = 64U;
constexpr size_t APP_CRASH_LOG_MAX_READ_SIZE = 100 * 1024;      // 100k
constexpr uint64_t APP_CRASH_WAIT_TIME = 5000;        // 5s waiting
constexpr const char* const SAND_FILE_APP_LOG_PATH_PREFIX = "/data/storage/el2/log/";
constexpr const char* const APP_CRASH_LOG_SUFFIX = "_CppCrash_AppMerge.log";

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
void SigAlarmCallBack()
{
    ProcessDumper::GetInstance().ReportSigDumpStats();
}
#endif
}

ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper ins;
    return ins;
}

void ProcessDumper::Dump()
{
    startTime_ = GetTimeMillisec();
    startAbsTime_ = GetAbsTimeMilliSeconds();
    DumpErrorCode resDump = DumpProcess();
    if (resDump == DumpErrorCode::DUMP_COREDUMP) {
        return;
    }
    FormatJsonInfoIfNeed(resDump);
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
        if (needMergeLog_ && mergeLogThread_.joinable()) {
            mergeLogThread_.join();
        }
        SysEventReporter reporter(request_.type);
        reporter.Report(*process_, request_);
    }
}

DumpErrorCode ProcessDumper::DumpProcess()
{
    DFX_TRACE_SCOPED("DumpProcess");
    DumpErrorCode dumpRes = ReadRequestAndCheck();
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
    dumpRes = DumpPreparation();
    if (dumpRes != DumpErrorCode::DUMP_ESUCCESS) {
        return dumpRes;
    }
    PrintDumpInfo(dumpRes);
    UnwindWriteJit();
    DfxPtrace::Detach(process_->GetVmPid());
    return dumpRes;
}

std::future<DumpErrorCode> ProcessDumper::AsyncInitialization()
{
    std::promise<void> createMapsFinish;
    createMapsFuture_ = createMapsFinish.get_future();
    return std::async(std::launch::async, [this, initFinish = std::move(createMapsFinish)] () mutable {
        dfxMaps_ = DfxMaps::Create(request_.nsPid, request_.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH, false);
        initFinish.set_value();
        if (!DfxBufferWriter::GetInstance().InitBufferWriter(request_)) {
            DFXLOGE("Failed to init buffer writer.");
            return DumpErrorCode::DUMP_EGETFD;
        }
        return DumpErrorCode::DUMP_ESUCCESS;
    });
}

DumpErrorCode ProcessDumper::DumpPreparation()
{
    UpdateConfigByRequest();
    if (!InitDfxProcess()) {
        DFXLOGE("Failed to init crash process info.");
        return DumpErrorCode::DUMP_EATTACH;
    }
    std::future<DumpErrorCode> initTask;
    bool isCoreDump = false;
#if defined(__aarch64__) && !defined(is_ohos_lite)
    isCoreDump = coredumpManager_ != nullptr;
#endif
    if (!isCoreDump) {
        initTask = AsyncInitialization();
    }
    if (ReadVmPid() <= 0) {
        ReportCrashException(CrashExceptionCode::CRASH_DUMP_EREADPID);
        DFXLOGE("Failed to read vm pid!");
        return DumpErrorCode::DUMP_EREADPID;
    }
#if defined(__aarch64__) && !defined(is_ohos_lite)
    if (coredumpManager_) {
        coredumpManager_->DumpMemoryForPid(process_->GetVmPid());
        return DumpErrorCode::DUMP_COREDUMP;
    }
#endif
    DumpErrorCode dumpRes = DumpErrorCode::DUMP_ESUCCESS;
    if (initTask.valid()) {
        dumpRes = initTask.get(); // wait init task finish
        if (dumpRes != DumpErrorCode::DUMP_ESUCCESS) {
            return dumpRes;
        }
    }
    if (!InitUnwinder(dumpRes)) {
        return dumpRes;
    }
    return DumpErrorCode::DUMP_ESUCCESS;
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

void ProcessDumper::FormatJsonInfoIfNeed(const DumpErrorCode& resDump)
{
    if (!isJsonDump_ && request_.type != ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        return;
    }
    if (process_ == nullptr) {
        return;
    }
    std::string jsonInfo;
    int dumpError = 0;
    switch (resDump) {
        case DUMP_ESUCCESS:
            dumpError = 0;
            break;
        case DUMP_ESYMBOL_NO_PARSE:
        case DUMP_ESYMBOL_PARSE_TIMEOUT:
            dumpError = 1;
            break;
        default:
            dumpError = -1;
            break;
    }
    DumpInfoJsonFormatter::GetJsonFormatInfo(request_, *process_, jsonInfo, dumpError);
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        process_->SetCrashInfoJson(jsonInfo);
    } else if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        DfxBufferWriter::GetInstance().WriteMsg(jsonInfo);
    }
    DFXLOGI("Finish GetJsonFormatInfo len %{public}" PRIuPTR "", jsonInfo.length());
}

void ProcessDumper::WriteDumpResIfNeed(const DumpErrorCode& resDump)
{
    DFXLOGI("dump result: %{public}s", DfxDumpRes::ToString(resDump).c_str());
    if (request_.siginfo.si_signo != SIGDUMP) {
        return;
    }
    if (!DfxBufferWriter::GetInstance().WriteDumpRes(resDump)) { // write dump res, retry request pipefd
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

DumpErrorCode ProcessDumper::ReadRequestAndCheck()
{
    DFX_TRACE_SCOPED("ReadRequestAndCheck");
    ElapsedTime counter("ReadRequestAndCheck", 20); // 20 : limit cost time 20 ms
    ssize_t readCount = OHOS_TEMP_FAILURE_RETRY(read(STDIN_FILENO, &request_, sizeof(ProcessDumpRequest)));
    request_.threadName[NAME_BUF_LEN - 1] = '\0';
    request_.processName[NAME_BUF_LEN - 1] = '\0';
    request_.appRunningUniqueId[MAX_APP_RUNNING_UNIQUE_ID_LEN - 1] = '\0';
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

void ProcessDumper::PrintDumpInfo(DumpErrorCode& dumpRes)
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
    auto dumpInfo = DumpInfoFactory::GetInstance().CreateObject(dumpInfoComponent[0]);
    auto prevDumpInfo = dumpInfo;
    std::vector<std::shared_ptr<DumpInfo>> dumpInfos;
    for (size_t index = 1; index < dumpInfoComponent.size(); index++) {
        dumpInfo = DumpInfoFactory::GetInstance().CreateObject(dumpInfoComponent[index]);
        if (dumpInfo == nullptr) {
            DFXLOGE("Failed to create object%{public}s.", dumpInfoComponent[index].c_str());
            continue;
        }
        dumpInfos.push_back(dumpInfo);
        dumpInfo->SetDumpInfo(prevDumpInfo);
        prevDumpInfo = dumpInfo;
    }
    if (dumpInfo == nullptr) {
        DFXLOGE("dumpInfo is nullptr.");
        return;
    }
    int unwindSuccessCnt = dumpInfo->UnwindStack(*process_, request_, *unwinder_);
    for (const auto& info : dumpInfos) {
        info->Collect(*process_, request_, *unwinder_);
    }

    DFXLOGI("unwind success thread count(%{public}d)", unwindSuccessCnt);
    if (request_.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        DumpUtils::InfoCrashUnwindResult(request_, unwindSuccessCnt > 0);
        DumpUtils::BlockCrashProcExit(request_);
        ReportToAbilityManagerService(*process_, request_);
    }
    if (!DumpUtils::IsSelinuxPermissive() && request_.uid > MIN_HAP_UID) {
        process_->Detach();  // crash scene
        crashDetachTime_ = GetTimeMillisec();
    }

    if (process_ != nullptr && process_->GetCrashLogConfig().mergeAppLog) {
        EnableMergeAppLog();
        mergeLogThread_ = std::thread(&ProcessDumper::ReadAppLog, this);
    }
    dumpRes = unwindSuccessCnt > 0 ? ConcurrentSymbolize() : DumpErrorCode::DUMP_ESTOPUNWIND;
    if (!isJsonDump_) { // isJsonDump_ will print after format json
        dumpInfo->Print(*process_, request_, *unwinder_);
    }
}

void ProcessDumper::ReadAppLog()
{
    std::string bundleName = DumpUtils::GetSelfBundleName();
    std::string logfile = std::string(SAND_FILE_APP_LOG_PATH_PREFIX) + bundleName + APP_CRASH_LOG_SUFFIX;

    uint64_t currentTime = GetTimeMillisec();
    uint64_t elapsedTime = (currentTime > crashDetachTime_) ? (currentTime - crashDetachTime_) : 0;
    if (elapsedTime < APP_CRASH_WAIT_TIME) {
        uint64_t leftTime = APP_CRASH_WAIT_TIME - elapsedTime;
        std::this_thread::sleep_for(std::chrono::milliseconds(leftTime));
    }

    mergeLogString_ = ReadFileWithTimeHeader(logfile);
    DFXLOGI("ReadAppLog finished. Path: %{public}s, Content size: %{public}zu",
            logfile.c_str(), mergeLogString_.size());
}

std::string ProcessDumper::GetFileModificationTime(const struct stat& fileInfo)
{
    char timeBuffer[TIME_BUFFER_SIZE] = {0};
    struct tm timeStruct = {0};
    if (localtime_r(&fileInfo.st_mtime, &timeStruct) != nullptr) {
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeStruct);
    }
    return "Last Modified: " + std::string(timeBuffer) + "\n";
}

std::string ProcessDumper::ReadFileContent(const std::string& filePath, size_t fileSize)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        DFXLOGE("Open file failed: %{public}s", filePath.c_str());
        return "";
    }

    size_t readLen = std::min(fileSize, static_cast<size_t>(APP_CRASH_LOG_MAX_READ_SIZE));
    std::string content;
    if (readLen > 0) {
        content.resize(readLen);
        file.read(&content[0], static_cast<std::streamsize>(readLen));
        content.resize(static_cast<size_t>(file.gcount()));
    }
    return content;
}

std::string ProcessDumper::ReadFileWithTimeHeader(const std::string& filePath)
{
    if (filePath.empty()) {
        return "Error: File path empty";
    }

    struct stat fileInfo = {0};
    if (stat(filePath.c_str(), &fileInfo) != 0) {
        std::string errMsg = "Error: File not found or stat failed. Path:" + filePath +
                             ", errno:" + std::to_string(errno) + ", desc: " + strerror(errno);
        DFXLOGE("readFileWithTimeHeader error msg:%{public}s", errMsg.c_str());
        return errMsg;
    }

    std::string result = GetFileModificationTime(fileInfo);
    result += ReadFileContent(filePath, static_cast<size_t>(fileInfo.st_size));
    return result;
}

DumpErrorCode ProcessDumper::WaitParseSymbols()
{
    uint64_t curTime = GetAbsTimeMilliSeconds();
    DumpErrorCode dumpRes = DumpErrorCode::DUMP_ESUCCESS;
#if defined(__aarch64__)
    uint32_t lessRemainTimeMs =
        static_cast<uint32_t>(ProcessDumpConfig::GetInstance().GetConfig().reservedParseSymbolTime);
    if (request_.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH || expectedDumpFinishTime_ == 0) {
        threadPool_.Stop();
    } else if (expectedDumpFinishTime_ > curTime && expectedDumpFinishTime_ - curTime > lessRemainTimeMs) {
        uint64_t waitTime = expectedDumpFinishTime_ - curTime - lessRemainTimeMs;
        if (!threadPool_.StopWithTimeOut(waitTime)) {
            dumpRes = DumpErrorCode::DUMP_ESYMBOL_PARSE_TIMEOUT;
        }
    } else {
        DFXLOGW("do not parse symbol, remain %{public}" PRId64 "ms", expectedDumpFinishTime_ - curTime);
        dumpRes = DumpErrorCode::DUMP_ESYMBOL_NO_PARSE;
    }
#else
    uint64_t unwindTime = curTime > startAbsTime_ ? curTime - startAbsTime_ : 0;
    uint64_t waitTime = (unwindTime > 0 && PROCESSDUMP_REMAIN_TIME > unwindTime) ?
        PROCESSDUMP_REMAIN_TIME - unwindTime : 0;
    if (request_.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
        threadPool_.Stop();
    } else if (waitTime > 0) {
        DFXLOGI("parse symbol, wait %{public}" PRId64 "ms", waitTime);
        if (!threadPool_.StopWithTimeOut(waitTime)) {
            dumpRes = DumpErrorCode::DUMP_ESYMBOL_PARSE_TIMEOUT;
        }
    } else {
        DFXLOGW("do not parse symbol, time not enough!");
        dumpRes = DumpErrorCode::DUMP_ESYMBOL_NO_PARSE;
    }
#endif
    finishParseSymbolTime_ = GetTimeMillisec();
    return dumpRes;
}

DumpErrorCode ProcessDumper::ConcurrentSymbolize()
{
    DumpErrorCode dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
    if (process_ == nullptr || process_->GetNativeFramesTable().empty()) {
        return dumpRes;
    }
    constexpr size_t threadPoolCapacity = 3;
    threadPool_.Start(threadPoolCapacity);
    using mapIterator = std::map<uint64_t, OHOS::HiviewDFX::DfxFrame>::iterator;
    auto addTask = [this] (mapIterator left, mapIterator right) {
        threadPool_.AddTask([this, left, right] () mutable {
            DFX_TRACE_SCOPED("ParseSymbol:%s", left->second.mapName.c_str());
            bool smoFlag = false;
            uint64_t start = 0;
            if (StartsWith(left->second.mapName, ADLT_PATH_NAME_START)) {
                smoFlag = true;
                start = GetTimeMillisec();
            }
            auto leftMap = left->second.map;
            while (left != right) {
                unwinder_->ParseFrameSymbol(left->second);
                left++;
            }
            if (leftMap != nullptr) {
                leftMap->ReleaseElf();
            }
            if (smoFlag) {
                smoParseTime_ += GetTimeMillisec() - start;
            }
        });
    };
    auto left = process_->GetNativeFramesTable().begin(); // left boundary of sliding window
    auto initialLeft = left;
    auto end = process_->GetNativeFramesTable().end();
    for (auto right = ++initialLeft; right != end; ++right) {
        // if map of right boundary of sliding window equal to left, right boundary moves right one step
        if (left->second.map == right->second.map) {
            continue;
        }
        // if not equal, add element of the sliding window to task
        addTask(left, right);
        left = right; // left moves to the location of right
    }
    addTask(left, end); // Add the element between left and end to the task
    dumpRes = WaitParseSymbols();
    FillAllThreadNativeSymbol();
    return dumpRes;
}

void ProcessDumper::FillAllThreadNativeSymbol()
{
    auto keyThread = process_->GetKeyThread();
    if (keyThread != nullptr) {
        keyThread->FillSymbol(process_->GetNativeFramesTable());
    }
    for (const auto &thread : process_->GetOtherThreads()) {
        if (thread != nullptr) {
            thread->FillSymbol(process_->GetNativeFramesTable());
        }
    }
}

bool ProcessDumper::InitUnwinder(DumpErrorCode &dumpRes)
{
    DFX_TRACE_SCOPED("InitUnwinder");
    if (process_ == nullptr) {
        DFXLOGE("Failed to read real pid!");
        return false;
    }
#if defined(PROCESSDUMP_MINIDEBUGINFO)
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
    bool crash = request_.type != ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    unwinder_ = std::make_shared<Unwinder>(process_->GetVmPid(), process_->GetVmPid(), crash, dfxMaps_);
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
    process_->SetLogSource("processdump");
    if (!process_->InitKeyThread(request_)) {
        DumpUtils::NotifyOperateResult(request_, OPE_FAIL);
        return false;
    }
    DFXLOGI("Init key thread successfully.");
#if defined(__aarch64__) && !defined(is_ohos_lite)
    if (CoredumpController::IsCoredumpAllowed(request_)) {
        coredumpManager_ = std::make_unique<CoredumpManager>();
        coredumpManager_->ProcessRequest(request_);
    }
#endif
    DumpUtils::NotifyOperateResult(request_, OPE_SUCCESS);
    process_->InitOtherThreads(request_);
    DFXLOGI("Finish create all thread.");
#if defined(__aarch64__) && !defined(is_ohos_lite)
    if (coredumpManager_) {
        coredumpManager_->TriggerCoredump();
    }
#endif
    return true;
}

int ProcessDumper::ReadVmPid()
{
    pid_t childPid = 0;
    DFX_TRACE_SCOPED("ReadVmPid");
    ptrace(PTRACE_SETOPTIONS, request_.tid, NULL, PTRACE_O_TRACEFORK);
    ptrace(PTRACE_CONT, request_.tid, 0, 0);
    DumpUtils::WaitForFork(request_.tid, childPid);
    ptrace(PTRACE_CONT, childPid, NULL, NULL);
    bool isCoredumpSignal = false;
#if defined(__aarch64__) && !defined(is_ohos_lite)
    isCoredumpSignal = CoredumpController::IsCoredumpSignal(request_);
#endif
    // freeze and coredump detach target processdump after fork child to fork vm process
    if (request_.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH || isCoredumpSignal) {
        if (createMapsFuture_.valid()) {
            createMapsFuture_.get();
        }
        process_->Detach();
        DFXLOGI("ptrace detach all tids");
    }
    pid_t sonPid = 0;
    DumpUtils::WaitForFork(childPid, sonPid);
    int vmPid = static_cast<int>(sonPid);
    ptrace(PTRACE_DETACH, childPid, 0, 0);
    process_->SetVmPid(vmPid);
    DFXLOGI("processdump get vm pid is %{public}d", vmPid);
    return vmPid;
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
    stat->smoParseTime = smoParseTime_;
    stat->keyThreadUnwindTimestamp = KeyThreadDumpInfo::GetUnwindTimestamp();
    if (IsBetaVersion()) {
        stat->pssMemory = GetPssMemory();
    }
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
