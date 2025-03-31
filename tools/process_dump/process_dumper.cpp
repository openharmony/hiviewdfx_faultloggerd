/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "cppcrash_reporter.h"
#include "crash_exception.h"
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_dump_res.h"
#include "dfx_fdsan.h"
#include "dfx_logger.h"
#include "dfx_ptrace.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_stack_info_json_formatter.h"
#include "dfx_thread.h"
#include "dfx_unwind_remote.h"
#include "dfx_util.h"
#include "dfx_trace.h"
#include "directory_ex.h"
#include "elapsed_time.h"
#include "faultloggerd_client.h"
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif
#include "info/fatal_message.h"
#include "printer.h"
#include "procinfo.h"
#include "unwinder_config.h"
#ifndef is_ohos_lite
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
const char *const GLOBAL_REGION = "const.global.region";
const char *const LOGSYSTEM_VERSION_TYPE = "const.logsystem.versiontype";
MAYBE_UNUSED const char *const MIXSTACK_ENABLE = "faultloggerd.priv.mixstack.enabled";
const int MAX_FILE_COUNT = 5;
const int ARG_MAX_NUM = 131072;

static bool IsOversea()
{
#ifndef is_ohos_lite
    static bool isOversea = OHOS::system::GetParameter(GLOBAL_REGION, "CN") != "CN";
    return isOversea;
#else
    return false;
#endif
}

static bool IsBetaVersion()
{
#ifndef is_ohos_lite
    static bool isBetaVersion = OHOS::system::GetParameter(LOGSYSTEM_VERSION_TYPE, "") == "beta";
    return isBetaVersion;
#else
    return true;
#endif
}

static bool IsBlockCrashProcess()
{
    bool isBlockCrash = false;
#ifndef is_ohos_lite
    isBlockCrash = OHOS::system::GetParameter(BLOCK_CRASH_PROCESS, "false") == "true";
#endif
    return isBlockCrash;
}

void WriteData(int fd, const std::string& data, size_t blockSize)
{
    size_t dataSize = data.length();
    size_t index = 0;
    while (index < dataSize) {
        size_t writeLength = (index + blockSize) <= dataSize ? blockSize : (dataSize - index);
        ssize_t nwrite = OHOS_TEMP_FAILURE_RETRY(write(fd, data.substr(index, writeLength).c_str(), writeLength));
        if (nwrite != static_cast<ssize_t>(writeLength)) {
            DFXLOGI("%{public}s :: nwrite: %{public}zd, writeLength: %{public}zu, errno:%{public}d", __func__, nwrite,
                writeLength, errno);
        }
        index += writeLength;
    }
    DFXLOGI("%{public}s :: needWriteDataSize: %{public}zu, writeDataSize: %{public}zu", __func__, dataSize, index);
}

using OpenFilesList = std::map<int, FDInfo>;

bool ReadLink(std::string &src, std::string &dst)
{
    char buf[PATH_MAX];
    ssize_t count = readlink(src.c_str(), buf, sizeof(buf) - 1);
    if (count < 0) {
        return false;
    }
    buf[count] = '\0';
    dst = buf;
    return true;
}

void CollectOpenFiles(OpenFilesList &list, pid_t pid)
{
    std::string fdDirName = "/proc/" + std::to_string(pid) + "/fd";
    std::unique_ptr<DIR, int (*)(DIR *)> dir(opendir(fdDirName.c_str()), closedir);
    if (dir == nullptr) {
        DFXLOGE("failed to open directory %{public}s: %{public}s", fdDirName.c_str(), strerror(errno));
        return;
    }

    struct dirent *de;
    while ((de = readdir(dir.get())) != nullptr) {
        if (*de->d_name == '.') {
            continue;
        }

        int fd = atoi(de->d_name);
        std::string path = fdDirName + "/" + std::string(de->d_name);
        std::string target;
        if (ReadLink(path, target)) {
            list[fd].path = target;
        } else {
            list[fd].path = "???";
            DFXLOGE("failed to readlink %{public}s: %{public}s", path.c_str(), strerror(errno));
        }
    }
}

void FillFdsaninfo(OpenFilesList &list, pid_t nsPid, uint64_t fdTableAddr)
{
    constexpr size_t fds = sizeof(FdTable::entries) / sizeof(*FdTable::entries);
    size_t entryOffset = offsetof(FdTable, entries);
    uint64_t addr = fdTableAddr + entryOffset;
    FdEntry entrys[fds];
    if (ReadProcMemByPid(nsPid, addr, entrys, sizeof(FdEntry) * fds) != sizeof(FdEntry) * fds) {
        DFXLOGE("[%{public}d]: read nsPid mem error %{public}s", __LINE__, strerror(errno));
        return;
    }
    for (size_t i = 0; i < fds; i++) {
        if (entrys[i].close_tag) {
            list[i].fdsanOwner = entrys[i].close_tag;
        }
    }

    size_t overflowOffset = offsetof(FdTable, overflow);
    uintptr_t overflow = 0;
    uint64_t tmp = fdTableAddr + overflowOffset;
    if (ReadProcMemByPid(nsPid, tmp, &overflow, sizeof(overflow)) != sizeof(overflow)) {
        return;
    }
    if (!overflow) {
        return;
    }

    size_t overflowLength;
    if (ReadProcMemByPid(nsPid, overflow, &overflowLength, sizeof(overflowLength))
        != sizeof(overflowLength)) {
        return;
    }
    if (overflowLength > ARG_MAX_NUM) {
        return;
    }

    std::vector<FdEntry> overflowFdEntrys(overflowLength);
    uint64_t address = overflow + offsetof(FdTableOverflow, entries);
    if (ReadProcMemByPid(nsPid, address, overflowFdEntrys.data(), sizeof(FdEntry) * overflowLength) !=
        sizeof(FdEntry) * overflowLength) {
        DFXLOGE("[%{public}d]: read nsPid mem error %{public}s", __LINE__, strerror(errno));
        return;
    }
    size_t fdIndex = fds;
    for (size_t i = 0; i < overflowLength; i++) {
        if (overflowFdEntrys[i].close_tag) {
            list[fdIndex].fdsanOwner = overflowFdEntrys[i].close_tag;
        }
        fdIndex++;
    }
}

std::string DumpOpenFiles(OpenFilesList &files)
{
    std::string openFiles = "OpenFiles:\n";
    for (auto &filePath: files) {
        const std::string path = filePath.second.path;
        uint64_t tag = filePath.second.fdsanOwner;
        const char* type = fdsan_get_tag_type(tag);
        uint64_t val = fdsan_get_tag_value(tag);
        if (!path.empty()) {
            openFiles += std::to_string(filePath.first) + "->" + path + " " + type + " " +
                std::to_string(val) + "\n";
        } else {
            openFiles += "OpenFilesList contain an entry (fd " +
                std::to_string(filePath.first) + ") with no path or owner\n";
        }
    }
    return openFiles;
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

void ReadVmRealPid(std::shared_ptr<ProcessDumpRequest> request, unsigned long vmPid, int& realPid)
{
    int waitStatus = 0;
    ptrace(PTRACE_SETOPTIONS, vmPid, NULL, PTRACE_O_TRACEEXIT); // block son exit
    ptrace(PTRACE_CONT, vmPid, NULL, NULL);

    long data = 0;
    DFXLOGI("start wait exit event happen");
    waitpid(vmPid, &waitStatus, 0); // wait exit stop
    data = ptrace(PTRACE_PEEKDATA, vmPid, reinterpret_cast<void *>(request->vmProcRealPidAddr), nullptr);
    if (data < 0) {
        DFXLOGI("ptrace peek data error %{public}lu %{public}d", vmPid, errno);
    }
    realPid = static_cast<int>(data);
}

void ReadPids(std::shared_ptr<ProcessDumpRequest> request, int& realPid, int& vmPid, std::shared_ptr<DfxProcess> proc)
{
    unsigned long childPid = 0;

    WaitForFork(request->tid, childPid);
    ptrace(PTRACE_CONT, childPid, NULL, NULL);

    // frezze detach after fork child to fork vm process
    if (request->siginfo.si_signo == SIGDUMP) {
        if (proc->keyThread_ != nullptr) {
            proc->keyThread_->Detach();
        }
        proc->Detach();
        DFXLOGI("ptrace detach all tids");
    }

    unsigned long sonPid = 0;
    WaitForFork(childPid, sonPid);
    vmPid = static_cast<int>(sonPid);

    ptrace(PTRACE_DETACH, childPid, 0, 0);
    ReadVmRealPid(request, sonPid, realPid);

    DFXLOGI("procecdump get real pid is %{public}d vm pid is %{public}d", realPid, vmPid);
}

void InfoRemoteProcessResult(std::shared_ptr<ProcessDumpRequest> request, int result)
{
    if (request == nullptr) {
        return;
    }
    if (request->childPipeFd[0] != -1) {
        close(request->childPipeFd[0]);
        request->childPipeFd[0] = -1;
    }
    if (OHOS_TEMP_FAILURE_RETRY(write(request->childPipeFd[1], &result, sizeof(result))) < 0) {
        DFXLOGE("write to child process fail %{public}d", errno);
    }
}

void SetProcessdumpTimeout(siginfo_t &si)
{
    if (si.si_signo != SIGDUMP) {
        return;
    }

    uint64_t endTime;
    int tid;
    ParseSiValue(si, endTime, tid);

    if (tid == 0) {
        DFXLOGI("reset, prevent incorrect reading sival_int for tid");
        si.si_value.sival_int = 0;
    }

    if (endTime == 0) {
        DFXLOGI("end time is zero, not set new alarm");
        return;
    }

    uint64_t curTime = GetAbsTimeMilliSeconds();
    if (curTime >= endTime) {
        DFXLOGI("now has timeout, processdump exit");
#ifndef CLANG_COVERAGE
        _exit(0);
#endif
    }
    uint64_t diffTime = endTime - curTime;

    DFXLOGI("processdump remain time%{public}" PRIu64 "ms", diffTime);
    if (diffTime > PROCESSDUMP_TIMEOUT * NUMBER_ONE_THOUSAND) {
        DFXLOGE("dump remain time is invalid, not set timer");
        return;
    }

    struct itimerval timer;
    timer.it_value.tv_sec = static_cast<int64_t>(diffTime / NUMBER_ONE_THOUSAND);
    timer.it_value.tv_usec = static_cast<int64_t>(diffTime * NUMBER_ONE_THOUSAND % NUMBER_ONE_MILLION);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
        DFXLOGE("start processdump timer fail %{public}d", errno);
    }
}

static bool IsDebugSignal(const siginfo_t& siginfo)
{
    if (siginfo.si_signo != SIGLEAK_STACK) {
        return false;
    }

    switch (siginfo.si_code) {
        case SIGLEAK_STACK_FDSAN:
            FALLTHROUGH_INTENDED;
        case SIGLEAK_STACK_JEMALLOC:
            FALLTHROUGH_INTENDED;
        case SIGLEAK_STACK_BADFD:
            return true;
        default:
            return false;
    }
}

void InfoCrashUnwindResult(const std::shared_ptr<ProcessDumpRequest> request, bool isUnwindSucc)
{
    if (!isUnwindSucc) {
        return;
    }
    if (ptrace(PTRACE_POKEDATA, request->nsPid, reinterpret_cast<void*>(request->unwindResultAddr),
        CRASH_UNWIND_SUCCESS_FLAG) < 0) {
        DFXLOGE("pok unwind success flag to nsPid %{public}d fail %{public}s", request->nsPid, strerror(errno));
    }
}

void BlockCrashProcExit(const std::shared_ptr<ProcessDumpRequest> request)
{
    if (!IsBlockCrashProcess()) {
        return;
    }
    DFXLOGI("start block crash process pid %{public}d nspid %{public}d", request->pid, request->nsPid);
    if (ptrace(PTRACE_POKEDATA, request->nsPid, reinterpret_cast<void*>(request->blockCrashExitAddr),
        CRASH_BLOCK_EXIT_FLAG) < 0) {
        DFXLOGE("pok block falg to nsPid %{public}d fail %{public}s", request->nsPid, strerror(errno));
    }
}

bool IsDumpSignal(int signo)
{
    return signo == SIGDUMP || signo == SIGLEAK_STACK;
}

bool IsLeakDumpSignal(int signo)
{
    return signo == SIGLEAK_STACK;
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
    std::shared_ptr<ProcessDumpRequest> request = std::make_shared<ProcessDumpRequest>();
    resDump_ = DumpProcess(request);
    std::string jsonInfo;
    if (isJsonDump_ || isCrash_) {
        DfxStackInfoJsonFormatter formatter(process_, request);
        formatter.GetJsonFormatInfo(isJsonDump_, jsonInfo);
        DFXLOGI("Finish GetJsonFormatInfo len %{public}" PRIuPTR "", jsonInfo.length());
        if (isJsonDump_) {
            WriteData(bufferFd_, jsonInfo, MAX_PIPE_SIZE);
            CloseFd(bufferFd_);
        }
    }

    finishTime_ = GetTimeMillisec();
    ReportSigDumpStats(request);

    if (request->siginfo.si_signo == SIGDUMP) {
        WriteDumpRes(resDump_, request->pid);
        CloseFd(resFd_);
    }

    // print keythread base info to hilog
    DfxRingBufferWrapper::GetInstance().PrintBaseInfo();
    DfxRingBufferWrapper::GetInstance().StopThread();
    DFXLOGI("Finish dump stacktrace for %{public}s(%{public}d:%{public}d).",
        request->processName, request->pid, request->tid);
    Report(request, jsonInfo);

    if (!IsDumpSignal(request->siginfo.si_signo)) {
        InfoCrashUnwindResult(request, resDump_ == DumpErrorCode::DUMP_ESUCCESS);
        BlockCrashProcExit(request);
    }
    if (process_ != nullptr) {
        if (process_->keyThread_ != nullptr) {
            process_->keyThread_->Detach();
        }
        process_->Detach();
    }
}

void ProcessDumper::Report(std::shared_ptr<ProcessDumpRequest> request, std::string &jsonInfo)
{
    if (request == nullptr) {
        DFXLOGE("request is nullptr.");
        return;
    }
    if (resDump_ == DumpErrorCode::DUMP_ENOMAP || resDump_ == DumpErrorCode::DUMP_EREADPID) {
        DFXLOGE("resDump_ is %{public}d.", resDump_);
        return;
    }
    if (IsDebugSignal(request->siginfo)) {
        ReportAddrSanitizer(*request, jsonInfo);
        return;
    }
    if (isCrash_ && request->siginfo.si_signo != SIGLEAK_STACK) {
        ReportCrashInfo(jsonInfo, *request);
    }
}

static int32_t ReadRequestAndCheck(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("ReadRequestAndCheck");
    ElapsedTime counter("ReadRequestAndCheck", 20); // 20 : limit cost time 20 ms
    ssize_t readCount = OHOS_TEMP_FAILURE_RETRY(read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest)));
    request->threadName[NAME_BUF_LEN - 1] = '\0';
    request->processName[NAME_BUF_LEN - 1] = '\0';
    request->msg.body[MAX_FATAL_MSG_SIZE - 1] = '\0';
    request->appRunningId[MAX_APP_RUNNING_UNIQUE_ID_LEN - 1] = '\0';
    if (readCount != static_cast<long>(sizeof(ProcessDumpRequest))) {
        DFXLOGE("Failed to read DumpRequest(%{public}d), readCount(%{public}zd).", errno, readCount);
        ReportCrashException(request->processName, request->pid, request->uid,
                             CrashExceptionCode::CRASH_DUMP_EREADREQ);
        return DumpErrorCode::DUMP_EREADREQUEST;
    }

    return DumpErrorCode::DUMP_ESUCCESS;
}

std::string GetOpenFiles(int32_t pid, int nsPid, uint64_t fdTableAddr)
{
    DFX_TRACE_SCOPED("GetOpenFiles");
    OpenFilesList openFies;
    CollectOpenFiles(openFies, pid);
    FillFdsaninfo(openFies, nsPid, fdTableAddr);
    std::string fds = DumpOpenFiles(openFies);
    DFXLOGI("get open files info finish");
    return fds;
}

void ProcessDumper::InitRegs(std::shared_ptr<ProcessDumpRequest> request, int &dumpRes)
{
    DFX_TRACE_SCOPED("InitRegs");
    if (!DfxUnwindRemote::GetInstance().InitProcessAllThreadRegs(request, process_)) {
        DFXLOGE("Failed to init process regs.");
        dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
        return;
    }

    DFXLOGI("get all tid regs finish");
}

void ProcessDumper::UnwindWriteJit(const ProcessDumpRequest &request)
{
    if (!isCrash_ || !unwinder_) {
        return;
    }

    const auto& jitCache = unwinder_->GetJitCache();
    if (jitCache.empty()) {
        return;
    }
    struct FaultLoggerdRequest jitRequest;
    (void)memset_s(&jitRequest, sizeof(jitRequest), 0, sizeof(jitRequest));
    jitRequest.type = FaultLoggerType::JIT_CODE_LOG;
    jitRequest.pid = request.pid;
    jitRequest.tid = request.tid;
    jitRequest.time = request.timeStamp;
    int32_t fd = RequestFileDescriptorEx(&jitRequest);
    if (fd == -1) {
        DFXLOGE("request jitlog fd failed.");
        return;
    }
    if (unwinder_->ArkWriteJitCodeToFile(fd) < 0) {
        DFXLOGE("jit code write file failed.");
    }
    (void)close(fd);
}

std::string ProcessDumper::ReadStringByPtrace(pid_t tid, uintptr_t addr, size_t maxLen)
{
    constexpr size_t maxReadLen = 1024 * 1024 * 1; // 1M
    if (maxLen == 0 || maxLen > maxReadLen) {
        DFXLOGE("maxLen(%{public}zu) is invalid value.", maxLen);
        return "";
    }
    size_t bufLen = (maxLen + sizeof(long) - 1) / sizeof(long);
    std::vector<long> buffer(bufLen, 0);
    for (size_t i = 0; i < bufLen; i++) {
        long ret = ptrace(PTRACE_PEEKTEXT, tid, reinterpret_cast<void*>(addr + sizeof(long) * i), nullptr);
        if (ret == -1) {
            DFXLOGE("read target mem by ptrace failed, errno(%{public}s).", strerror(errno));
            break;
        }
        buffer[i] = ret;
        if (ret == 0) {
            break;
        }
    }
    char* val = reinterpret_cast<char*>(buffer.data());
    val[maxLen - 1] = '\0';
    return std::string(val);
}

void ProcessDumper::UpdateFatalMessageWhenDebugSignal(const ProcessDumpRequest& request, pid_t vmPid)
{
    pid_t pid = vmPid != 0 ? vmPid : request.nsPid;
    if (request.siginfo.si_signo != SIGLEAK_STACK ||
        !(request.siginfo.si_code == SIGLEAK_STACK_FDSAN || request.siginfo.si_code == SIGLEAK_STACK_JEMALLOC)) {
        return;
    }

    if (pid == 0 || process_ == nullptr) {
        DFXLOGE("pid %{public}d, process_ is %{public}s nullptr.", pid, process_ == nullptr ? "" : "not");
        return;
    }

    auto debugMsgPtr = reinterpret_cast<uintptr_t>(request.siginfo.si_value.sival_ptr);
    debug_msg_t dMsg = {0};
    if (ReadProcMemByPid(pid, debugMsgPtr, &dMsg, sizeof(dMsg)) != sizeof(debug_msg_t)) {
        DFXLOGE("Get debug_msg_t failed.");
        return;
    }
    auto msgPtr = reinterpret_cast<uintptr_t>(dMsg.msg);
    auto lastMsg = ReadStringByPtrace(pid, msgPtr, MAX_FATAL_MSG_SIZE);
    if (lastMsg.empty()) {
        DFXLOGE("Get debug_msg_t.msg failed.");
        return;
    }
    process_->SetFatalMessage(lastMsg);
}

std::string ProcessDumper::ReadCrashObjString(pid_t tid, uintptr_t addr) const
{
    DFXLOGI("Start read string type of crashObj.");
    std::string stringContent = "ExtraCrashInfo(String):\n";
    constexpr int bufLen = 256;
    auto readStr = ReadStringByPtrace(tid, addr, sizeof(long) * bufLen);
    stringContent += (readStr + "\n");
    return stringContent;
}

std::string ProcessDumper::ReadCrashObjMemory(pid_t tid, uintptr_t addr, size_t length) const
{
    DFXLOGI("Start read memory type of crashObj, memory length(%zu).", length);
    constexpr size_t step = sizeof(uintptr_t);
    std::string memoryContent = StringPrintf("ExtraCrashInfo(Memory start address %018" PRIx64 "):",
        static_cast<uint64_t>(addr));
    size_t size = (length + step - 1) / step;
    std::vector<uintptr_t> memory(size, 0);
    if (ReadProcMemByPid(tid, addr, memory.data(), length) != length) {
        DFXLOGE("[%{public}d]: read target mem error %{public}s", __LINE__, strerror(errno));
        memoryContent += "\n";
        return memoryContent;
    }
    for (size_t index = 0; index < size; index++) {
        size_t offset = index * step;
        if (offset % 0x20 == 0) {  // Print offset every 32 bytes
            memoryContent += StringPrintf("\n+0x%03" PRIx64 ":", static_cast<uint64_t>(offset));
        }
        memoryContent += StringPrintf(" %018" PRIx64, static_cast<uint64_t>(memory[index]));
    }
    memoryContent += "\n";
    return memoryContent;
}

void ProcessDumper::GetCrashObj(std::shared_ptr<ProcessDumpRequest> request)
{
#ifdef __LP64__
    if (!isCrash_ || request->crashObj == 0 || process_ == nullptr) {
        return;
    }
    uintptr_t type = request->crashObj >> 56; // 56 :: Move 56 bit to the right
    uintptr_t addr = request->crashObj & 0xffffffffffffff;
    std::vector<size_t> memorylengthTable = {0, 64, 256, 1024, 2048, 4096};
    if (type == 0) {
        process_->extraCrashInfo += ReadCrashObjString(request->nsPid, addr);
    } else if (type < memorylengthTable.size()) {
        process_->extraCrashInfo += ReadCrashObjMemory(request->nsPid, addr, memorylengthTable[type]);
    }
#endif
}

bool ProcessDumper::Unwind(std::shared_ptr<ProcessDumpRequest> request, int &dumpRes, pid_t vmPid)
{
    // dump unwind should still keep main thread or aim thread is frist unwind
    if (!isCrash_) {
        int tid = request->siginfo.si_value.sival_int;
        tid = tid != 0 ? tid : request->nsPid;
        for (auto &thread : process_->GetOtherThreads()) {
            if (thread->threadInfo_.nsTid == tid) {
                swap(process_->keyThread_, thread);
                break;
            }
        }
    }
    GetCrashObj(request);
    if (!IsOversea() || IsBetaVersion()) {
        UpdateFatalMessageWhenDebugSignal(*request, vmPid);
    }
    if (!DfxUnwindRemote::GetInstance().UnwindProcess(request, process_, unwinder_, vmPid)) {
        DFXLOGE("Failed to unwind process.");
        dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
        return false;
    }
    return true;
}

void ProcessDumper::UnwindFinish(std::shared_ptr<ProcessDumpRequest> request, pid_t vmPid)
{
    if (process_ == nullptr) {
        DFXLOGE("Dump process failed, please check permission and whether pid is valid.");
        return;
    }

    DfxUnwindRemote::GetInstance().ParseSymbol(request, process_, unwinder_);
    DfxUnwindRemote::GetInstance().PrintUnwindResultInfo(request, process_, unwinder_, vmPid);
    UnwindWriteJit(*request);
}

int ProcessDumper::DumpProcess(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("DumpProcess");
    int dumpRes = DumpErrorCode::DUMP_ESUCCESS;
    do {
        if ((dumpRes = ReadRequestAndCheck(request)) != DumpErrorCode::DUMP_ESUCCESS) {
            break;
        }
        SetProcessdumpTimeout(request->siginfo);
        isCrash_ = request->siginfo.si_signo != SIGDUMP;
        DFXLOGI("Processdump SigVal(%{public}d), TargetPid(%{public}d:%{public}d), TargetTid(%{public}d), " \
            "threadname(%{public}s).",
            request->siginfo.si_value.sival_int, request->pid, request->nsPid, request->tid, request->threadName);

        if (InitProcessInfo(request) < 0) {
            DFXLOGE("Failed to init crash process info.");
            dumpRes = DumpErrorCode::DUMP_EATTACH;
            break;
        }
        InitRegs(request, dumpRes);
        pid_t vmPid = 0;
        if (!InitUnwinder(request, vmPid, dumpRes) && (isCrash_ && !IsLeakDumpSignal(request->siginfo.si_signo))) {
            break;
        }
        if (InitPrintThread(request) < 0) {
            DFXLOGE("Failed to init print thread.");
            dumpRes = DumpErrorCode::DUMP_EGETFD;
        }
        ReadFdTable(*request);
        if (!Unwind(request, dumpRes, vmPid)) {
            DFXLOGE("unwind fail.");
        }
        UnwindFinish(request, vmPid);
        DfxPtrace::Detach(vmPid);
    } while (false);
    return dumpRes;
}

bool ProcessDumper::InitKeyThread(std::shared_ptr<ProcessDumpRequest> request)
{
    if (request == nullptr || process_ == nullptr) {
        return false;
    }
    pid_t nsTid = request->tid;
    pid_t tid = process_->ChangeTid(nsTid, true);
    process_->keyThread_ = DfxThread::Create(process_->processInfo_.pid, tid, nsTid);
    if ((process_->keyThread_ == nullptr) || (!process_->keyThread_->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT))) {
        DFXLOGE("Failed to attach key thread(%{public}d).", nsTid);
        ReportCrashException(request->processName, request->pid, request->uid,
                             CrashExceptionCode::CRASH_DUMP_EATTACH);
        if (!isCrash_) {
            return false;
        }
    }
    if (process_->keyThread_ != nullptr) {
        InfoRemoteProcessResult(request, OPE_SUCCESS);
        ptrace(PTRACE_SETOPTIONS, process_->keyThread_->threadInfo_.nsTid, NULL, PTRACE_O_TRACEFORK);
        ptrace(PTRACE_CONT, process_->keyThread_->threadInfo_.nsTid, 0, 0);
    }

    if ((process_->keyThread_ != nullptr) && process_->keyThread_->threadInfo_.threadName.empty()) {
        process_->keyThread_->threadInfo_.threadName = std::string(request->threadName);
    }
    return true;
}

bool ProcessDumper::InitUnwinder(std::shared_ptr<ProcessDumpRequest> request, pid_t &vmPid, int &dumpRes)
{
    DFX_TRACE_SCOPED("InitUnwinder");
    pid_t realPid = 0;
    ReadPids(request, realPid, vmPid, process_);
    if (realPid == 0 || vmPid == 0) {
        ReportCrashException(request->processName, request->pid, request->uid,
            CrashExceptionCode::CRASH_DUMP_EREADPID);
        DFXLOGE("Failed to read real pid!");
        dumpRes = DumpErrorCode::DUMP_EREADPID;
        return false;
    }

    unwinder_ = std::make_shared<Unwinder>(realPid, vmPid, isCrash_);
    if (unwinder_ == nullptr) {
        DFXLOGE("unwinder_ is nullptr!");
        return false;
    }
    if (unwinder_->GetMaps() == nullptr) {
        ReportCrashException(request->processName, request->pid, request->uid,
            CrashExceptionCode::CRASH_LOG_EMAPLOS);
        DFXLOGE("Mapinfo of crashed process is not exist!");
        dumpRes = DumpErrorCode::DUMP_ENOMAP;
        return false;
    }
    return true;
}

int ProcessDumper::InitProcessInfo(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("InitProcessInfo");
    if (request->pid <= 0) {
        return -1;
    }
    process_ = DfxProcess::Create(request->pid, request->nsPid);
    if (process_ == nullptr) {
        return -1;
    }
    if (process_->processInfo_.processName.empty()) {
        process_->processInfo_.processName = std::string(request->processName);
    }
    process_->processInfo_.uid = request->uid;
    process_->recycleTid_ = request->recycleTid;
    if (request->msg.type == MESSAGE_FATAL) {
        if (!IsOversea() || IsBetaVersion()) {
            process_->SetFatalMessage(request->msg.body);
        }
    } else if (request->msg.type == MESSAGE_CALLBACK) {
        process_->extraCrashInfo += StringPrintf("ExtraCrashInfo(Callback):\n%s\n", request->msg.body);
    }

    if (!InitKeyThread(request)) {
        return -1;
    }

    DFXLOGI("ptrace attach all tids");
    if (!IsLeakDumpSignal(request->siginfo.si_signo)) {
        process_->InitOtherThreads();
        process_->Attach();
    }
#if defined(PROCESSDUMP_MINIDEBUGINFO)
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
    return 0;
}

int ProcessDumper::GetLogTypeByRequest(const ProcessDumpRequest &request)
{
    switch (request.siginfo.si_signo) {
        case SIGLEAK_STACK:
            switch (request.siginfo.si_code) {
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

int32_t ProcessDumper::CreateFileForCrash(int32_t pid, uint64_t time) const
{
    const std::string logFilePath = "/log/crash";
    const std::string logFileType = "cppcrash";
    const int32_t logcrashFileProp = 0644; // 0640:-rw-r--r--
    if (access(logFilePath.c_str(), F_OK) != 0) {
        DFXLOGE("%{public}s is not exist.", logFilePath.c_str());
        return INVALID_FD;
    }
    std::string logPath = logFilePath + "/" + logFileType + "-" + std::to_string(pid) + "-" + std::to_string(time);
    int32_t fd = OHOS_TEMP_FAILURE_RETRY(open(logPath.c_str(), O_RDWR | O_CREAT, logcrashFileProp));
    if (fd == INVALID_FD) {
        DFXLOGE("create %{public}s failed, errno=%{public}d", logPath.c_str(), errno);
    } else {
        DFXLOGI("create crash path %{public}s succ.", logPath.c_str());
    }
    return fd;
}

void ProcessDumper::RemoveFileIfNeed()
{
    const std::string logFilePath = "/log/crash";
    std::vector<std::string> files;
    OHOS::GetDirFiles(logFilePath, files);
    if (files.size() < MAX_FILE_COUNT) {
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

    int deleteNum = static_cast<int>(files.size()) - (MAX_FILE_COUNT - 1);
    for (int index = 0; index < deleteNum; index++) {
        DFXLOGI("Now we delete file(%{public}s) due to exceed file max count.", files[index].c_str());
        OHOS::RemoveFile(files[index]);
    }
}

int ProcessDumper::InitPrintThread(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("InitPrintThread");
    struct FaultLoggerdRequest faultloggerdRequest;
    (void)memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest));
    faultloggerdRequest.type = ProcessDumper::GetLogTypeByRequest(*request);
    faultloggerdRequest.pid = request->pid;
    faultloggerdRequest.tid = request->tid;
    faultloggerdRequest.time = request->timeStamp;
    if (isCrash_ || faultloggerdRequest.type == FaultLoggerType::LEAK_STACKTRACE) {
        bufferFd_ = RequestFileDescriptorEx(&faultloggerdRequest);
        if (bufferFd_ == -1) {
            ProcessDumper::RemoveFileIfNeed();
            bufferFd_ = CreateFileForCrash(request->pid, request->timeStamp);
        }
        DfxRingBufferWrapper::GetInstance().SetWriteFunc(ProcessDumper::WriteDumpBuf);
    } else {
        isJsonDump_ = request->siginfo.si_code == DUMP_TYPE_REMOTE_JSON;
        int pipeWriteFd[] = { -1, -1 };
        if (RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd) == 0) {
            bufferFd_ = pipeWriteFd[PIPE_BUF_INDEX];
            resFd_ = pipeWriteFd[PIPE_RES_INDEX];
        }
    }

    if (bufferFd_ < 0) {
        DFXLOGW("Failed to request fd from faultloggerd.");
        ReportCrashException(request->processName, request->pid, request->uid,
                             CrashExceptionCode::CRASH_DUMP_EWRITEFD);
    }
    int result = bufferFd_;
    if (!isJsonDump_) {
        DfxRingBufferWrapper::GetInstance().SetWriteBufFd(bufferFd_);
        bufferFd_ = INVALID_FD; // bufferFd_ set to INVALID_FD to prevent double close fd
    }
    DfxRingBufferWrapper::GetInstance().StartThread();
    return result;
}

int ProcessDumper::WriteDumpBuf(int fd, const char* buf, const int len)
{
    if (buf == nullptr) {
        return -1;
    }
    return WriteLog(fd, "%s", buf);
}

void ProcessDumper::WriteDumpRes(int32_t res, pid_t pid)
{
    DFXLOGI("%{public}s :: res: %{public}s", __func__, DfxDumpRes::ToString(res).c_str());
    // After skipping InitPrintThread due to ptrace failure or other reasons,
    // retry request resFd_ to write back the result to dumpcatch
    if (resFd_ < 0) {
        int pipeWriteFd[] = { -1, -1 };
        if (RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd) == -1) {
            DFXLOGE("%{public}s request pipe failed, err:%{public}d", __func__, errno);
            return;
        }
        CloseFd(pipeWriteFd[0]);
        resFd_ = pipeWriteFd[1];
    }

    ssize_t nwrite = OHOS_TEMP_FAILURE_RETRY(write(resFd_, &res, sizeof(res)));
    if (nwrite < 0) {
        DFXLOGE("%{public}s write fail, err:%{public}d", __func__, errno);
    }
}

bool ProcessDumper::IsCrash() const
{
    return isCrash_;
}

void ProcessDumper::ReportSigDumpStats(const std::shared_ptr<ProcessDumpRequest> &request) const
{
    if (isCrash_) {
        return;
    }

    std::vector<uint8_t> buf(sizeof(struct FaultLoggerdStatsRequest), 0);
    auto stat = reinterpret_cast<struct FaultLoggerdStatsRequest*>(buf.data());
    stat->type = PROCESS_DUMP;
    stat->pid = request->pid;
    stat->signalTime = request->timeStamp;
    stat->processdumpStartTime = startTime_;
    stat->processdumpFinishTime = finishTime_;
    if (memcpy_s(stat->targetProcess, sizeof(stat->targetProcess),
        request->processName, sizeof(request->processName)) != 0) {
        DFXLOGE("Failed to copy target processName (%{public}d)", errno);
        return;
    }

    ReportDumpStats(stat);
}

void ProcessDumper::ReportCrashInfo(const std::string& jsonInfo, const ProcessDumpRequest &request)
{
    if (process_ == nullptr) {
        return;
    }
    auto reporter = CppCrashReporter(request.timeStamp, process_, request.dumpMode);
    reporter.SetCppCrashInfo(jsonInfo);
    reporter.ReportToHiview();
    reporter.ReportToAbilityManagerService();
}

void ProcessDumper::ReportAddrSanitizer(ProcessDumpRequest &request, std::string &jsonInfo)
{
#ifndef HISYSEVENT_DISABLE
    std::string reason = "DEBUG SIGNAL";
    std::string summary;
    if (process_ != nullptr && process_->keyThread_ != nullptr) {
        reason = process_->reason;
        summary = process_->keyThread_->ToString();
    }
    HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::RELIABILITY, "ADDR_SANITIZER",
                    OHOS::HiviewDFX::HiSysEvent::EventType::FAULT,
                    "MODULE", request.processName,
                    "PID", request.pid,
                    "UID", request.uid,
                    "HAPPEN_TIME", request.timeStamp,
                    "REASON", reason,
                    "SUMMARY", summary);
    DFXLOGI("%{public}s", "Report fdsan event done.");
#else
    DFXLOGI("%{public}s", "Not supported for fdsan reporting.");
#endif
}

void ProcessDumper::ReadFdTable(const ProcessDumpRequest &request)
{
    // Non crash, leak, jemalloc do not read fdtable
    if (!isCrash_ ||
        (request.siginfo.si_signo == SIGLEAK_STACK &&
         request.siginfo.si_code != SIGLEAK_STACK_FDSAN &&
         request.siginfo.si_code != SIGLEAK_STACK_BADFD)) {
        return;
    }
    process_->openFiles = GetOpenFiles(request.pid, request.nsPid, request.fdTableAddr);
}
} // namespace HiviewDFX
} // namespace OHOS
