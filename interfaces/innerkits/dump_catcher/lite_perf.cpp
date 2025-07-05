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
#include "dfx_define.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "dfx_lperf.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#undef LOG_TAG
#define LOG_TAG "DfxLitePerf"

static constexpr int DUMP_LITEPERF_TIMEOUT_DELAY = 3000;
}

class LitePerf::Impl {
public:
    int StartProcessStackSampling(const std::vector<int>& tids, int freq, int durationMs, bool parseMiniDebugInfo);
    int CollectSampleStackByTid(int tid, std::string& stack);
    int FinishProcessStackSampling();

private:
    int ExecDump(const std::vector<int>& tids, int freq, int durationMs, bool parseMiniDebugInfo);
    bool ExecDumpPipe(const int (&pipefd)[2], const LitePerfParam& lperf);

    int DumpPoll(const int (&pipeFds)[2], const int timeout);
    bool HandlePollEvents(const struct pollfd (&readFds)[2], const int (&pipeFds)[2], bool& bPipeConnect, int& pollRet);
    bool DoReadBuf(int fd);
    bool DoReadRes(int fd, int& pollRet);
    bool ParseSampleStacks(const std::string& source);

    std::string bufMsg_;
    std::string resMsg_;
    std::map<int, std::string> stackMaps_{};
    std::mutex mutex_;
    std::atomic<bool> isRunning_{false};
};

LitePerf::LitePerf() : impl_(std::make_shared<Impl>())
{}

int LitePerf::StartProcessStackSampling(const std::vector<int>& tids, int freq, int durationMs, bool parseMiniDebugInfo)
{
    return impl_->StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
}

int LitePerf::CollectSampleStackByTid(int tid, std::string& stack)
{
    return impl_->CollectSampleStackByTid(tid, stack);
}

int LitePerf::FinishProcessStackSampling()
{
    return impl_->FinishProcessStackSampling();
}

int LitePerf::Impl::StartProcessStackSampling(const std::vector<int>& tids, int freq, int durationMs,
    bool parseMiniDebugInfo)
{
    DFXLOGI("StartProcessStackSampling.");
    int res = 0;
    if (tids.size() > MAX_PERF_TIDS_SIZE || durationMs < 1) {
        DFXLOGE("Failed to tids size.");
        return -1;
    }
    bool expected = false;
    if (!isRunning_.compare_exchange_strong(expected, true)) {
        DFXLOGW("Process is being sampling.");
        return -1;
    }

    int pipeReadFd[] = {-1, -1};
    int req = RequestLitePerfPipeFd(FaultLoggerPipeType::PIPE_FD_READ, pipeReadFd);
    if (req != ResponseCode::REQUEST_SUCCESS) {
        DFXLOGE("Failed to request liteperf pipe read.");
        isRunning_.store(false);
        res = -1;
        return res;
    }

    do {
        if (ExecDump(tids, freq, durationMs, parseMiniDebugInfo) < 0) {
            res = -1;
            break;
        }

        int timeout = durationMs + DUMP_LITEPERF_TIMEOUT_DELAY;
        if (DumpPoll(pipeReadFd, timeout) < 0) {
            res = -1;
            break;
        }
    } while (false);

    req = RequestLitePerfDelPipeFd();
    if (req != ResponseCode::REQUEST_SUCCESS) {
        DFXLOGE("Failed to request liteperf pipe delete.");
    }
    isRunning_.store(false);
    return res;
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
    return pollRet;
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

        if (bPipeConnect &&
            ((static_cast<uint32_t>(readFd.revents) & POLLERR) || (static_cast<uint32_t>(readFd.revents) & POLLHUP))) {
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

int LitePerf::Impl::ExecDump(const std::vector<int>& tids, int freq, int durationMs, bool parseMiniDebugInfo)
{
    LitePerfParam lperf;
    lperf.pid = getpid();
    for (size_t i = 0; i < tids.size() && i < MAX_PERF_TIDS_SIZE; ++i) {
        lperf.tids[i] = tids[i];
    }
    lperf.freq = freq;
    lperf.durationMs = durationMs;
    lperf.parseMiniDebugInfo = parseMiniDebugInfo;

    pid_t pid = 0;
    pid = vfork();
    if (pid < 0) {
        DFXLOGE("Failed to vfork.");
        return -1;
    }
    if (pid == 0) {
        int pipefd[PIPE_NUM_SZ] = {-1, -1};
        if (pipe2(pipefd, O_NONBLOCK) != 0) {
            DFXLOGE("Failed to create pipe, errno: %{public}d.", errno);
            _exit(-1);
        }

        pid_t dumpPid = vfork();
        if (dumpPid < 0) {
            DFXLOGE("Failed to vfork.");
            _exit(-1);
        }
        if (dumpPid == 0) {
            ExecDumpPipe(pipefd, lperf);
            _exit(0);
        } else {
            _exit(0);
        }
    }
    int res = waitpid(pid, nullptr, 0);
    if (res < 0) {
        DFXLOGE("Failed to wait pid(%{public}d), errno(%{public}d)", pid, errno);
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

    DFXLOGI("execl processdump -liteperf.");
    execl(PROCESSDUMP_PATH, "processdump", "-liteperf", nullptr);
    DFXLOGE("Failed to execl processdump -liteperf, errno(%{public}d)", errno);
    return false;
}

bool LitePerf::Impl::ParseSampleStacks(const std::string& source)
{
    if (source.empty()) {
        return false;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    if (stackMaps_.empty()) {
        std::vector<std::string> stacks;
        OHOS::SplitStr(source, LITE_PERF_SPLIT, stacks);
        for (auto& str : stacks) {
            size_t pos = str.find("\n");
            if (pos  == std::string::npos) {
                DFXLOGW("error str: %s", str.c_str());
                continue;
            }

            std::string tidStr = str.substr(0, pos);
            int tmpTid;
            int ret = sscanf_s(tidStr.c_str(), "%d", &tmpTid);
            if (ret != 1) {
                DFXLOGE("sscanf %{public}s failed.", tidStr.c_str());
                continue;
            }
            std::string stack = str.substr(pos + 1);
            DFXLOGD("emplace tid:%{public}d, str: %s", tmpTid, stack.c_str());
            stackMaps_.emplace(tmpTid, std::move(stack));
        }
    }
    return (stackMaps_.size() > 0);
}

int LitePerf::Impl::CollectSampleStackByTid(int tid, std::string& stack)
{
    DFXLOGI("CollectSampleStackByTid, tid:%{public}d.", tid);
    if (!ParseSampleStacks(bufMsg_)) {
        return -1;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    if (stackMaps_.find(tid) != stackMaps_.end()) {
        stack = stackMaps_[tid];
        if (stack.size() > 0) {
            return 0;
        }
    }
    return -1;
}

int LitePerf::Impl::FinishProcessStackSampling()
{
    DFXLOGI("FinishProcessStackSampling.");
    std::unique_lock<std::mutex> lck(mutex_);
    stackMaps_.clear();
    bufMsg_.clear();
    resMsg_.clear();
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS