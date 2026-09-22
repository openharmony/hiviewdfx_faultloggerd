/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "pipe_poller.h"

#include <securec.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "dump_catcher_constants.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "PipePoller"
#endif

namespace OHOS {
namespace HiviewDFX {

bool PipePoller::HandlePollError(uint64_t endTime, int& remainTime, int& pollRet, std::string& resMsg)
{
    if (errno == EINTR) {
        uint64_t now = GetAbsTimeMilliSeconds();
        if (now >= endTime) {
            pollRet = DUMP_POLL_TIMEOUT;
            resMsg.append("Result: poll timeout.\n");
            return false;
        }
        remainTime = static_cast<int>(endTime - now);
        return true;
    }
    pollRet = DUMP_POLL_FAILED;
    resMsg.append("Result: poll error, errno(" + std::to_string(errno) + ")\n");
    return false;
}

bool PipePoller::HandlePollTimeout(uint64_t endTime, int& remainTime, int& pollRet, std::string& resMsg)
{
    uint64_t now = GetAbsTimeMilliSeconds();
    if (now < endTime) {
        remainTime = static_cast<int>(endTime - now);
        return true;
    }
    pollRet = DUMP_POLL_TIMEOUT;
    resMsg.append("Result: poll timeout.\n");
    return false;
}

bool PipePoller::HandlePollEvents(int pid, const struct pollfd (&readFds)[POLL_FD_NUM],
    bool& bPipeConnect, int& pollRet, DumpCatcherPipeData& pipeData)
{
    bool bufRet = true;
    bool resRet = false;
    bool eventRet = true;
    bool hasHup = false;
    for (auto& readFd : readFds) {
        bool hasPollIn = (static_cast<uint32_t>(readFd.revents) & POLLIN) != 0;
        if (!bPipeConnect && hasPollIn) {
            bPipeConnect = true;
        }
        if (!hasPollIn) {
            bool hasErr = (static_cast<uint32_t>(readFd.revents) & POLLERR) != 0;
            bool hasHupEvt = (static_cast<uint32_t>(readFd.revents) & POLLHUP) != 0;
            if (bPipeConnect && (hasErr || hasHupEvt)) {
                eventRet = false;
                pipeData.MutableResMsg().append("Result: poll events error.\n");
                break;
            }
            continue;
        }
        if (readFd.fd == pipeData.GetBufFd().GetFd()) {
            bufRet = pipeData.ReadBuf();
        } else if (readFd.fd == pipeData.GetResFd().GetFd()) {
            resRet = pipeData.ReadRes(pollRet);
        }
        if ((static_cast<uint32_t>(readFd.revents) & POLLHUP) != 0) {
            hasHup = true;
        }
    }
    if (!eventRet || !bufRet || resRet || hasHup) {
        DFXLOGI("eventRet:%{public}d bufRet:%{public}d resRet:%{public}d hasHup:%{public}d",
            eventRet, bufRet, resRet, hasHup);
        return false;
    }
    return true;
}

int PipePoller::DoPollLoop(int pid, int timeout, DumpCatcherPipeData& pipeData)
{
    int pollRet = DUMP_POLL_INIT;
    struct pollfd readFds[POLL_FD_NUM];
    (void)memset_s(readFds, sizeof(readFds), 0, sizeof(readFds));
    readFds[0].fd = pipeData.GetBufFd().GetFd();
    readFds[0].events = POLLIN;
    readFds[1].fd = pipeData.GetResFd().GetFd();
    readFds[1].events = POLLIN;
    bool bPipeConnect = false;
    int p90 = DumpCatcherConstants::REMOTE_P90_TIMEOUT_MS;
    int remainTime = p90 < timeout ? p90 : timeout;
    uint64_t endTime = GetAbsTimeMilliSeconds() + static_cast<uint64_t>(timeout);
    bool hasCollectStack = false;
    bool isContinue = true;
    do {
        int pRet = poll(readFds, POLL_FD_NUM, remainTime);
        if (pRet <= 0 && !hasCollectStack) {
            state_.notifyCollect = state_.stackKit.NotifyStartCollect(pid);
            hasCollectStack = true;
        }
        if (pRet < 0) {
            isContinue = HandlePollError(endTime, remainTime, pollRet, pipeData.MutableResMsg());
            continue;
        } else if (pRet == 0) {
            isContinue = HandlePollTimeout(endTime, remainTime, pollRet, pipeData.MutableResMsg());
            continue;
        }
        if (!HandlePollEvents(pid, readFds, bPipeConnect, pollRet, pipeData)) {
            break;
        }
        uint64_t now = GetAbsTimeMilliSeconds();
        if (now >= endTime) {
            pollRet = DUMP_POLL_TIMEOUT;
            pipeData.MutableResMsg().append("Result: poll timeout.\n");
            break;
        }
        remainTime = static_cast<int>(endTime - now);
    } while (isContinue);
    return pollRet;
}

int PipePoller::Poll(int pid, int timeout, DumpCatcherPipeData& pipeData)
{
    return DoPollLoop(pid, timeout, pipeData);
}

int PipePoller::HandleReadPhase(int pid, int timeout, DumpCatcherPipeData& pipeData,
    bool isJson, std::string& msg)
{
    if (!pipeData.IsValid()) {
        if (!isJson) {
            msg = "Result: bufFd or resFd < 0.\n";
        }
        DFXLOGE("invalid bufFd or resFd");
        return DUMP_POLL_FD;
    }
    int res = DoPollLoop(pid, timeout, pipeData);
    bool isDumpSuccess = (res == DUMP_POLL_OK) || (res == DUMP_POLL_NO_PARSE_SYMBOL) ||
        (res == DUMP_POLL_PARSE_SYMBOL_TIMEOUT);
    DFXLOGI("HandleReadPhase :: %{public}s", pipeData.GetResMsg().c_str());
    msg = isJson && isDumpSuccess ? pipeData.GetBufMsg() : (pipeData.GetResMsg() + pipeData.GetBufMsg());
    return res;
}

} // namespace HiviewDFX
} // namespace OHOS
