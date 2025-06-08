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

#include "dfx_sigdump_handler.h"

#include <cinttypes>
#include <csignal>
#include <ctime>
#include <mutex>
#include <securec.h>
#include <sigchain.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

#include "backtrace_local.h"
#include "dfx_define.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "faultloggerd_client.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSigDumpHandler"
#endif
}

static const struct timespec SIG_WAIT_TIMEOUT = {
    .tv_sec = 1,
    .tv_nsec = 0,
};

class DfxSigDumpHandler {
public:
    static DfxSigDumpHandler& GetInstance(void);
    bool Init(void);
    void Deinit(void);
    bool IsThreadRunning(void) const;
    int GetRunThreadId(void) const;
    void SetRunThreadId(int tid);
private:
    DfxSigDumpHandler() = default;
    DfxSigDumpHandler(DfxSigDumpHandler&) = delete;
    DfxSigDumpHandler& operator=(const DfxSigDumpHandler&)=delete;
    static void RunThread(void);
    static void SignalDumpRetranHandler(int signo, siginfo_t* si, void* context);
    bool isThreadRunning_{false};
    int runThreadId_{0};
};

DfxSigDumpHandler& DfxSigDumpHandler::GetInstance()
{
    static DfxSigDumpHandler sigDumperHandler;
    return sigDumperHandler;
}

bool DfxSigDumpHandler::IsThreadRunning() const
{
    return isThreadRunning_;
}

int DfxSigDumpHandler::GetRunThreadId() const
{
    return runThreadId_;
}

void DfxSigDumpHandler::SetRunThreadId(int tid)
{
    runThreadId_ = tid;
}

void DfxSigDumpHandler::SignalDumpRetranHandler(int signo, siginfo_t* si, void* context)
{
    int tid = DfxSigDumpHandler::GetInstance().GetRunThreadId();
    if (tid == 0) {
        return;
    }
    if (syscall(SYS_tkill, tid, SIGDUMP) != 0) {
        return;
    }
}

void DfxSigDumpHandler::RunThread()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGDUMP);
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0) {
        DFXLOGE("pthread sigmask failed, err(%{public}d)", errno);
    }
    DfxSigDumpHandler::GetInstance().SetRunThreadId(gettid());
    while (DfxSigDumpHandler::GetInstance().IsThreadRunning()) {
        siginfo_t si;
        if (OHOS_TEMP_FAILURE_RETRY(sigtimedwait(&set, &si, &SIG_WAIT_TIMEOUT)) == -1) {
            continue;
        }

        int res = DUMP_ESUCCESS;
        int32_t pid = getpid();
        int pipeWriteFd[] = { -1, -1 };
        if (RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd) == -1) {
            DFXLOGE("Pid %{public}d RequestPipeFd Failed", pid);
            continue;
        }
        SmartFd bufFd(pipeWriteFd[0]);
        SmartFd resFd(pipeWriteFd[1]);

        std::string dumpInfo = OHOS::HiviewDFX::GetProcessStacktrace();
        const ssize_t nwrite = static_cast<ssize_t>(dumpInfo.length());
        if (!dumpInfo.empty() &&
                OHOS_TEMP_FAILURE_RETRY(write(bufFd.GetFd(), dumpInfo.data(), dumpInfo.length())) != nwrite) {
            DFXLOGE("Pid %{public}d Write Buf Pipe Failed(%{public}d), nwrite(%{public}zd)", pid, errno, nwrite);
            res = DUMP_EBADFRAME;
        } else if (dumpInfo.empty()) {
            res = DUMP_ENOINFO;
        }
        ssize_t nres = OHOS_TEMP_FAILURE_RETRY(write(resFd.GetFd(), &res, sizeof(res)));
        if (nres != sizeof(res)) {
            DFXLOGE("Pid %{public}d Write Res Pipe Failed(%{public}d), nres(%{public}zd)", pid, errno, nres);
        }
    }
}

bool DfxSigDumpHandler::Init()
{
    if (IsThreadRunning()) {
        DFXLOGI("SigDumpHandler Thread has been inited");
        return true;
    }
    remove_all_special_handler(SIGDUMP);
    struct sigaction action;
    (void)memset_s(&action, sizeof(action), 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGDUMP);
    action.sa_flags = SA_RESTART | SA_SIGINFO;
    action.sa_sigaction = DfxSigDumpHandler::SignalDumpRetranHandler;
    DFXLOGI("Init Install signal handler");
    sigaction(SIGDUMP, &action, nullptr);
    isThreadRunning_ = true;
    std::thread catchThread = std::thread(&DfxSigDumpHandler::RunThread);
    catchThread.detach();
    return true;
}

void DfxSigDumpHandler::Deinit()
{
    isThreadRunning_ = false;
}
} // namespace HiviewDFX
} // namespace OHOS

bool InitSigDumpHandler()
{
    return OHOS::HiviewDFX::DfxSigDumpHandler::GetInstance().Init();
}

void DeinitSigDumpHandler()
{
    OHOS::HiviewDFX::DfxSigDumpHandler::GetInstance().Deinit();
}
