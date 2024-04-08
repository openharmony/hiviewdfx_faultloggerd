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

#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>

#include "crash_validator.h"
#include "ipc_skeleton.h"

static std::shared_ptr<OHOS::HiviewDFX::CrashValidator> g_validator = nullptr;
static void SigIntHandler()
{
    constexpr time_t breakTime = 5;
    static time_t lastHandleTime = 0;
    auto now = time(nullptr);
    if (now - lastHandleTime < breakTime) {
        raise(SIGTERM);
    } else {
        if (g_validator != nullptr) {
            g_validator->Dump(1);
        }
    }
    lastHandleTime = now;
}

int main(int argc, char *argv[])
{
    sigset_t waitMask;
    sigemptyset(&waitMask);
    sigaddset(&waitMask, SIGINT);
    sigprocmask(SIG_SETMASK, &waitMask, nullptr);
    g_validator = std::make_shared<OHOS::HiviewDFX::CrashValidator>();
    if (g_validator != nullptr) {
        g_validator->InitSysEventListener();
    }
    int sig = 0;
    int ret = -1;
    do {
        ret = sigwait(&waitMask, &sig);
        SigIntHandler();
    } while (ret == 0);
    return 0;
}
