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
#include <sigchain.h>
#include <thread>
#include <unistd.h>

#include "backtrace_local.h"
#include "dfx_define.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "faultloggerd_client.h"

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
    bool IsThreadRunning(void);
private:
    DfxSigDumpHandler() = default;
    DfxSigDumpHandler(DfxSigDumpHandler&) = delete;
    DfxSigDumpHandler& operator=(const DfxSigDumpHandler&)=delete;
    static void RunThread(void);
    bool isThreadRunning_{false};
};

DfxSigDumpHandler& DfxSigDumpHandler::GetInstance()
{
    static DfxSigDumpHandler sigDumperHandler;
    return sigDumperHandler;
}

bool DfxSigDumpHandler::IsThreadRunning()
{
    return isThreadRunning_;
}

void DfxSigDumpHandler::RunThread()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGDUMP);
    while (DfxSigDumpHandler::GetInstance().IsThreadRunning()) {
        siginfo_t si;
        if (OHOS_TEMP_FAILURE_RETRY(sigtimedwait(&set, &si, &SIG_WAIT_TIMEOUT)) == -1) {
            continue;
        }
        int32_t resFd = -1;
        int res = DUMP_ESUCCESS;
        int32_t pid = getpid();
        FaultLoggerPipeType jsonType = FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES;
        int32_t fd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_BUF);
        if (fd < 0) {
            fd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_WRITE_BUF);
            jsonType = FaultLoggerPipeType::PIPE_FD_WRITE_RES;
        }
        if (fd < 0) {
            DFXLOG_ERROR("Pid %d GetPipeFd Failed", pid);
            continue;
        }
        resFd = RequestPipeFd(pid, jsonType);
        if (resFd < 0) {
            DFXLOG_ERROR("Pid %d GetPipeResFd Failed", pid);
            close(fd);
            continue;
        }
        std::string dumpInfo = OHOS::HiviewDFX::GetProcessStacktrace();
        const ssize_t nwrite = static_cast<ssize_t>(dumpInfo.length());
        if (!dumpInfo.empty() &&
                OHOS_TEMP_FAILURE_RETRY(write(fd, dumpInfo.data(), dumpInfo.length())) != nwrite) {
            DFXLOG_ERROR("Pid %d Write Buf Pipe Failed(%d), nwrite(%zd)", pid, errno, nwrite);
            res = DUMP_EBADFRAME;
        } else if (dumpInfo.empty()) {
            res = DUMP_ENOINFO;
        }
        ssize_t nres = OHOS_TEMP_FAILURE_RETRY(write(resFd, &res, sizeof(res)));
        if (nres != sizeof(res)) {
            DFXLOG_ERROR("Pid %d Write Res Pipe Failed(%d), nres(%zd)", pid, errno, nres);
        }
        close(fd);
        close(resFd);
    }
}

bool DfxSigDumpHandler::Init()
{
    if (IsThreadRunning()) {
        DFXLOG_INFO("%s", "SigDumpHandler Thread has been inited");
        return true;
    }
    remove_all_special_handler(SIGDUMP);
    sigset_t set;
    if (pthread_sigmask(SIG_BLOCK, nullptr, &set) != 0) {
        DFXLOG_ERROR("pthread sigmask failed, err(%d)", errno);
        return false;
    }
    sigaddset(&set, SIGDUMP);
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0) {
        DFXLOG_ERROR("pthread sigmask failed, err(%d)", errno);
        return false;
    }
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