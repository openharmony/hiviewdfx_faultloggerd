/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "lite_perf.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <poll.h>
#include <securec.h>
#include <string_ex.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "dfx_lperf.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "procinfo.h"
#include "proc_util.h"
#include "smart_fd.h"
#include "stack_printer.h"
#include "unwinder.h"
#include "unwinder_config.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#undef LOG_TAG
#define LOG_TAG "DfxLitePerf"
}

extern "C" void DFX_RestoreDumpableState(void) __attribute__((weak));
extern "C" bool DFX_SetDumpableState(void) __attribute__((weak));
extern "C" int32_t DFX_InheritCapabilities(void) __attribute__((weak));

class LitePerf::Impl {
public:
    PerfErrorCode StartProcessStackSampling(const LitePerfConfig& config, bool checkLimit);
    int CollectSampleStackByTid(int tid, std::string& stack, int& count);
    int GetPerfCountByTid(pid_t tid);
    int FinishProcessStackSampling();
private:
    int ExecDump(const std::vector<int>& tids, int freq, int durationMs);
    bool InitDumpParam(const std::vector<int>& tids, int freq, int durationMs, LitePerfParam& lperf);
    bool ExecDumpPipe(const int (&pipefd)[2], const LitePerfParam& lperf);
    void FinishDump();

    int DumpPoll(const int (&pipeFds)[2], const int timeout);
    bool HandlePollEvents(const struct pollfd (&readFds)[2], const int (&pipeFds)[2], bool& bPipeConnect, int& pollRet);
    bool DoReadBuf(int fd);
    bool DoReadRes(int fd, int& pollRet);
    bool ParseSampleStacks(const std::string& datas);
    int WaitpidTimeout(pid_t pid);

    std::string bufMsg_;
    std::string resMsg_;
    std::map<int, std::pair<std::string, int>> stackMaps_{};
    std::mutex mutex_;
    std::atomic<bool> isRunning_{false};

    bool defaultEnableDebugInfo_ {false};
    bool enableDebugInfoSymbolic_ {false};
};

LitePerf::LitePerf() : impl_(std::make_shared<Impl>())
{}

int LitePerf::StartProcessStackSampling(const std::vector<int>& tids, int freq, int durationMs, bool parseMiniDebugInfo)
{
    if (tids.size() < MIN_SAMPLE_TIDS || tids.size() > MAX_SAMPLE_TIDS ||
        freq < MIN_SAMPLE_FREQUENCY || freq > MAX_SAMPLE_FREQUENCY ||
        durationMs < MIN_STOP_SECONDS || durationMs > MAX_STOP_SECONDS) {
        return -1;
    }
    return impl_->StartProcessStackSampling({
        .tids = tids,
        .freq = freq,
        .durationMs = durationMs,
        .parseMiniDebugInfo = parseMiniDebugInfo,
    }, false) == PerfErrorCode::SUCCESS ? 0 : -1;
}

int LitePerf::CollectSampleStackByTid(int tid, std::string& stack)
{
    int count = 0;
    return impl_->CollectSampleStackByTid(tid, stack, count);
}

int LitePerf::FinishProcessStackSampling()
{
    return impl_->FinishProcessStackSampling();
}

inline uint64_t GetThreadCpuTimes(pid_t tid)
{
    ProcessInfo process{};
    if (ParseThreadInfo(tid, process)) {
        return process.utime + process.stime;
    }
    return 0;
}

std::map<pid_t, uint64_t> ParseThreadTimes(const std::vector<int>& tids)
{
    std::map<pid_t, uint64_t> cpuTimesMap;
    for (auto tid : tids) {
        cpuTimesMap[tid] = GetThreadCpuTimes(tid);
    }
    return cpuTimesMap;
}

PerfErrorCode LitePerf::CollectProcessStackSampling(const LitePerfConfig& config, bool checkLimit, std::string& stacks)
{
    auto cpuTimesMap = ParseThreadTimes(config.tids);
    auto retCode = impl_->StartProcessStackSampling(config, checkLimit);
    if (retCode != PerfErrorCode::SUCCESS) {
        return retCode;
    }
    for (auto tid : config.tids) {
        stacks.append("Tid: " + std::to_string(tid) + ", ThreadName: ");
        char threadName[NAME_BUF_LEN];
        if (GetThreadName(threadName, NAME_BUF_LEN)) {
            stacks.append(std::string(threadName));
        }
        int count = 0;
        stacks.append(", Cputime: ");
        constexpr int secondToMillionSecond  = 1000;
        int64_t jiffsToMs = secondToMillionSecond / sysconf(_SC_CLK_TCK);
        stacks.append(std::to_string((GetThreadCpuTimes(tid) - cpuTimesMap[tid]) * jiffsToMs));
        stacks.append("ms, Count: ");
        std::string stackContent = "unknow";
        if (impl_->CollectSampleStackByTid(tid, stackContent, count) != 0) {
            DFXLOGI("Failed CollectSampleStackByTid, tid:%{public}d.", tid);
            continue;
        }
        stacks.append(std::to_string(count));
        stacks.append("\n");
        stacks.append(stackContent);
        stacks.append("\n");
    }
    impl_->FinishProcessStackSampling();
    return PerfErrorCode::SUCCESS;
}

PerfErrorCode LitePerf::Impl::StartProcessStackSampling(const LitePerfConfig& config, bool checkLimit)
{
    DFXLOGI("StartProcessStackSampling.");
    bool expected = false;
    if (!isRunning_.compare_exchange_strong(expected, true)) {
        DFXLOGW("Process is being sampling.");
        return PerfErrorCode::PERF_SAMPING;
    }
    auto autoResetFlag = std::unique_ptr<std::atomic<bool>, void(*)(std::atomic<bool>*)>(&isRunning_,
        [] (std::atomic<bool>* flag) {
            flag->store(false);
        });
    enableDebugInfoSymbolic_ = config.parseMiniDebugInfo;
    int timeout = config.durationMs + DUMP_LITEPERF_TIMEOUT;
    int pipeReadFd[] = {-1, -1};
    constexpr int secondsToMillionSecond = 1000;
    switch (RequestLitePerfPipeFd(FaultLoggerPipeType::PIPE_FD_READ, pipeReadFd,
        (timeout / secondsToMillionSecond), checkLimit)) {
        case ResponseCode::REQUEST_SUCCESS: {
            SmartFd bufFd(pipeReadFd[PIPE_BUF_INDEX], false);
            SmartFd resFd(pipeReadFd[PIPE_RES_INDEX], false);
            PerfErrorCode res = PerfErrorCode::SUCCESS;
            if (ExecDump(config.tids, config.freq, config.durationMs) < 0 || DumpPoll(pipeReadFd, timeout) < 0) {
                res = PerfErrorCode::UN_SUPPORTED;
            }
            FinishDump();
            return res;
        }
        case ResponseCode::SDK_DUMP_REPEAT:
            return PerfErrorCode::PERF_SAMPING;
        case ResponseCode::RESOURCE_LIMIT:
            return PerfErrorCode::RESOURCE_NOT_AVAILABLE;
        default:
            return PerfErrorCode::UN_SUPPORTED;
    }
}

void LitePerf::Impl::FinishDump()
{
    if (DFX_RestoreDumpableState == nullptr) {
        DFXLOGE("Failed to restore dumpable state.");
        return;
    }
    DFX_RestoreDumpableState();
    int req = RequestLitePerfDelPipeFd();
    if (req != ResponseCode::REQUEST_SUCCESS) {
        DFXLOGE("Failed to request liteperf pipe delete.");
    }
}

int LitePerf::Impl::DumpPoll(const int (&pipeFds)[2], const int timeout)
{
    MAYBE_UNUSED int pollRet = DUMP_POLL_INIT;
    struct pollfd readFds[2] = {};
    readFds[0].fd = pipeFds[PIPE_BUF_INDEX];
    readFds[0].events = POLLIN;
    readFds[1].fd = pipeFds[PIPE_RES_INDEX];
    readFds[1].events = POLLIN;
    int fdsSize = sizeof(readFds) / sizeof(readFds[0]);
    bool bPipeConnect = false;
    uint64_t endTime = GetAbsTimeMilliSeconds() + static_cast<uint64_t>(timeout);
    bool isContinue = true;
    do {
        uint64_t now = GetAbsTimeMilliSeconds();
        if (now >= endTime) {
            pollRet = DUMP_POLL_TIMEOUT;
            resMsg_.append("Result: poll timeout.\n");
            isContinue = false;
            break;
        }
        int remainTime = static_cast<int>(endTime - now);

        int pRet = poll(readFds, fdsSize, remainTime);
        if (pRet < 0) {
            if (errno == EINTR) {
                continue;
            }
            pollRet = DUMP_POLL_FAILED;
            resMsg_.append("Result: poll error, errno(" + std::to_string(errno) + ")\n");
            isContinue = false;
            break;
        } else if (pRet == 0) {
            pollRet = DUMP_POLL_TIMEOUT;
            resMsg_.append("Result: poll timeout.\n");
            isContinue = false;
            break;
        }

        if (!HandlePollEvents(readFds, pipeFds, bPipeConnect, pollRet)) {
            isContinue = false;
            break;
        }
    } while (isContinue);
    DFXLOGI("%{public}s :: %{public}s", __func__, resMsg_.c_str());
    return pollRet == DUMP_POLL_OK ? 0 : -1;
}

bool LitePerf::Impl::HandlePollEvents(const struct pollfd (&readFds)[2], const int (&pipeFds)[2],
    bool& bPipeConnect, int& pollRet)
{
    bool bufRet = true;
    bool resRet = false;
    bool eventRet = true;
    for (auto& readFd : readFds) {
        if (!bPipeConnect && (static_cast<uint32_t>(readFd.revents) & POLLIN)) {
            bPipeConnect = true;
        }

        if (bPipeConnect && ((static_cast<uint32_t>(readFd.revents) & POLLERR))) {
            resMsg_.append("Result: poll events error.\n");
            eventRet = false;
            break;
        }

        if ((static_cast<uint32_t>(readFd.revents) & POLLIN) != POLLIN) {
            continue;
        }

        if (readFd.fd == pipeFds[PIPE_BUF_INDEX]) {
            bufRet = DoReadBuf(pipeFds[PIPE_BUF_INDEX]);
        } else if (readFd.fd == pipeFds[PIPE_RES_INDEX]) {
            resRet = DoReadRes(pipeFds[PIPE_RES_INDEX], pollRet);
        }
    }
    if ((eventRet == false) || (bufRet == false) || (resRet == true)) {
        DFXLOGI("eventRet:%{public}d bufRet:%{public}d resRet:%{public}d", eventRet, bufRet, resRet);
        return false;
    }
    return true;
}

bool LitePerf::Impl::DoReadBuf(int fd)
{
    std::vector<char> buffer(MAX_PIPE_SIZE, 0);
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(fd, buffer.data(), MAX_PIPE_SIZE));
    if (nread <= 0) {
        DFXLOGW("%{public}s :: read error", __func__);
        return false;
    }
    DFXLOGI("%{public}s :: nread: %{public}zu", __func__, nread);
    bufMsg_.append(buffer.data(), static_cast<size_t>(nread));
    return true;
}

bool LitePerf::Impl::DoReadRes(int fd, int& pollRet)
{
    int32_t res = DumpErrorCode::DUMP_ESUCCESS;
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(fd, &res, sizeof(res)));
    if (nread <= 0 || nread != sizeof(res)) {
        DFXLOGW("%{public}s :: read error", __func__);
        return false;
    }
    pollRet = (res == DUMP_ESUCCESS) ? DUMP_POLL_OK : DUMP_POLL_FAILED;
    resMsg_.append("Result: " + DfxDumpRes::ToString(res) + "\n");
    return true;
}

bool LitePerf::Impl::InitDumpParam(const std::vector<int>& tids, int freq, int durationMs, LitePerfParam& lperf)
{
    (void)memset_s(&lperf, sizeof(LitePerfParam), 0, sizeof(LitePerfParam));
    int tidSize = 0;
    lperf.pid = getpid();
    for (size_t i = 0; i < tids.size() && i < MAX_SAMPLE_TIDS; ++i) {
        if (tids[i] <= 0 || !IsThreadInPid(lperf.pid, tids[i])) {
            DFXLOGW("%{public}s :: tid(%{public}d) error", __func__, tids[i]);
            continue;
        }
        lperf.tids[i] = tids[i];
        tidSize++;
    }

    if (tidSize <= 0) {
        DFXLOGE("%{public}s :: all tids error", __func__);
        return false;
    }
    lperf.freq = freq;
    lperf.durationMs = durationMs;

    if (DFX_SetDumpableState == nullptr || DFX_SetDumpableState() == false) {
        DFXLOGE("%{public}s :: Failed to set dumpable.", __func__);
        return false;
    }
    return true;
}

int LitePerf::Impl::WaitpidTimeout(pid_t pid)
{
    constexpr int waitCount = 150;
    for (int i = 0; i <= waitCount; i++) {
        int result = waitpid(pid, nullptr, WNOHANG);
        if (result == pid) {
            return 0;
        }
        if (result == -1) {
            DFXLOGE("Failed to wait pid(%{public}d)", pid);
            kill(pid, SIGKILL);
            return -1;
        }

        constexpr time_t usleepTime = 100000;
        usleep(usleepTime);
    }
    DFXLOGE("Failed to wait pid(%{public}d), timeout", pid);
    kill(pid, SIGKILL);
    return -1;
}

static pid_t ForkBySyscall(void)
{
#ifdef SYS_fork
    return syscall(SYS_fork);
#else
    return syscall(SYS_clone, SIGCHLD, 0);
#endif
}

int LitePerf::Impl::ExecDump(const std::vector<int>& tids, int freq, int durationMs)
{
    static LitePerfParam lperf;
    if (!InitDumpParam(tids, freq, durationMs, lperf)) {
        return -1;
    }

    pid_t pid = ForkBySyscall();
    if (pid < 0) {
        DFXLOGE("Failed to fork.");
        return -1;
    }
    if (pid == 0) {
        int pipefd[PIPE_NUM_SZ] = {-1, -1};
        if (pipe2(pipefd, O_NONBLOCK) != 0) {
            DFXLOGE("Failed to create pipe, errno: %{public}d.", errno);
            _exit(-1);
        }

        pid_t dumpPid = ForkBySyscall();
        if (dumpPid < 0) {
            DFXLOGE("Failed to fork.");
            _exit(-1);
        }
        if (dumpPid == 0) {
            ExecDumpPipe(pipefd, lperf);
            _exit(0);
        } else {
            _exit(0);
        }
    }
    int res = WaitpidTimeout(pid);
    if (res < 0) {
        DFXLOGE("Failed to wait pid(%{public}d), errno(%{public}d)", pid, errno);
        return -1;
    } else {
        DFXLOGI("wait pid(%{public}d) exit", pid);
    }
    return 0;
}

bool LitePerf::Impl::ExecDumpPipe(const int (&pipefd)[2], const LitePerfParam& lperf)
{
    ssize_t writeLen = sizeof(LitePerfParam);
    if (fcntl(pipefd[1], F_SETPIPE_SZ, writeLen) < writeLen) {
        DFXLOGE("Failed to set pipe buffer size, errno(%{public}d).", errno);
        return false;
    }

    struct iovec iovs[1] = {
        {
            .iov_base = (void *)(&lperf),
            .iov_len = sizeof(struct LitePerfParam)
        },
    };

    if (OHOS_TEMP_FAILURE_RETRY(writev(pipefd[PIPE_WRITE], iovs, 1)) != writeLen) {
        DFXLOGE("Failed to write pipe, errno(%{public}d)", errno);
        return false;
    }
    OHOS_TEMP_FAILURE_RETRY(dup2(pipefd[PIPE_READ], STDOUT_FILENO));
    if (pipefd[PIPE_READ] != STDOUT_FILENO) {
        close(pipefd[PIPE_READ]);
    }
    close(pipefd[PIPE_WRITE]);

    if (DFX_InheritCapabilities == nullptr || DFX_InheritCapabilities() != 0) {
        DFXLOGE("Failed to inherit Capabilities from parent.");
        return false;
    }

    DFXLOGI("execl processdump -liteperf.");
    execl(PROCESSDUMP_PATH, "processdump", "-liteperf", nullptr);
    DFXLOGE("Failed to execl processdump -liteperf, errno(%{public}d)", errno);
    return false;
}

bool LitePerf::Impl::ParseSampleStacks(const std::string& datas)
{
    if (datas.empty()) {
        return false;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    if (!stackMaps_.empty()) {
        return true;
    }
    auto unwinder = std::make_shared<Unwinder>(false);
    auto maps = DfxMaps::Create();
    defaultEnableDebugInfo_ = UnwinderConfig::GetEnableMiniDebugInfo();
    UnwinderConfig::SetEnableMiniDebugInfo(enableDebugInfoSymbolic_);
    std::istringstream iss(datas);
    auto frameMap = StackPrinter::DeserializeSampledFrameMap(iss);
    for (const auto& pair : frameMap) {
        auto stack = StackPrinter::PrintTreeStackBySampledStack(pair.second, false, unwinder, maps);
        int count = 0;
        for (auto& frame : pair.second) {
            if (frame.indent == 0) {
                count += frame.count;
            }
        }
        stackMaps_.emplace(pair.first, std::make_pair(std::move(stack), count));
    }
    return !stackMaps_.empty();
}

int LitePerf::Impl::CollectSampleStackByTid(int tid, std::string& stack, int& count)
{
    DFXLOGI("CollectSampleStackByTid, tid:%{public}d.", tid);
    if (!ParseSampleStacks(bufMsg_)) {
        return -1;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    auto stackInterator =  stackMaps_.find(tid);
    if (stackInterator != stackMaps_.end()) {
        stack = stackInterator->second.first;
        count = stackInterator->second.second;
        if (!stack.empty()) {
            return 0;
        }
    }
    return -1;
}

int LitePerf::Impl::FinishProcessStackSampling()
{
    DFXLOGI("FinishProcessStackSampling.");
    std::unique_lock<std::mutex> lck(mutex_);
    UnwinderConfig::SetEnableMiniDebugInfo(defaultEnableDebugInfo_);
    stackMaps_.clear();
    bufMsg_.clear();
    resMsg_.clear();
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS