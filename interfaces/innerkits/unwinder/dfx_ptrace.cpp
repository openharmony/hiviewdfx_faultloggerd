/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "dfx_ptrace.h"
#include <chrono>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxPtrace"
}

void DfxPtrace::Detach(pid_t tid)
{
    if (tid > 0) {
        ptrace(PTRACE_CONT, tid, 0, 0);
        ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
    }
}

bool DfxPtrace::Attach(pid_t tid, int timeout)
{
    if (tid <= 0) {
        return false;
    }

    if (ptrace(PTRACE_SEIZE, tid, 0, 0) != 0) {
        LOGW("Failed to seize tid(%d), errno=%d", tid, errno);
        return false;
    }

    if (ptrace(PTRACE_INTERRUPT, tid, 0, 0) != 0) {
        LOGW("Failed to interrupt tid(%d), errno=%d", tid, errno);
        ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
        return false;
    }

    int64_t startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    do {
        if (waitpid(tid, nullptr, WNOHANG) > 0) {
            break;
        }
        int64_t curTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (curTime - startTime > timeout) {
            ptrace(PTRACE_DETACH, tid, nullptr, nullptr);
            LOGW("Failed to wait tid(%d) attached.", tid);
            return false;
        }
        usleep(5); // 5 : sleep 5us
    } while (true);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
