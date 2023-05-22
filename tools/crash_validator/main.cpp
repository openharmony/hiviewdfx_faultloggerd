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

/* This files contains process dump entry function. */

#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>

#include "crash_validator.h"
#include "ipc_skeleton.h"

static bool g_needExit = false;
static std::condition_variable g_cv;
static std::mutex g_printLock;
static std::shared_ptr<OHOS::HiviewDFX::CrashValidator> g_validator = nullptr;
static void SigIntHandler(int sig)
{
    constexpr time_t breakTime = 5;
    static time_t lastHandleTime = 0;
    auto now = time(nullptr);
    if (now - lastHandleTime < breakTime) {
        g_needExit = true;
        g_cv.notify_one();
    }

    lastHandleTime = now;
    g_cv.notify_one();
}

static void PrinterThread()
{
    do {
        std::unique_lock<std::mutex> lk(g_printLock);
        g_cv.wait(lk);
        if (g_needExit) {
            raise(SIGTERM);
            break;
        }

        if (g_validator != nullptr) {
            g_validator->Dump(1);
        }
    } while (!g_needExit);
}

int main(int argc, char *argv[])
{
    g_validator = std::make_shared<OHOS::HiviewDFX::CrashValidator>();
    g_validator->InitSysEventListener();
    signal(SIGINT, SigIntHandler);
    std::thread thread(PrinterThread);
    OHOS::IPCSkeleton::GetInstance().JoinWorkThread();
    return 0;
}
