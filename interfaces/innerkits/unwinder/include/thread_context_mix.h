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

#ifndef THREAD_CONTEXT_MIX_H
#define THREAD_CONTEXT_MIX_H

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <csignal>
#include "dfx_maps.h"
#include "dfx_log.h"
#include <mutex>
#include <nocopyable.h>
#include <securec.h>
#include <string>

#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
constexpr int STACK_BUFFER_SIZE = 64 * 1024;
enum SyncStatus : int32_t {
    INIT = -1,
    WAIT_CTX = 0,
    COPY_START,
    COPY_SUCCESS,
};

struct ThreadContextSupportMix {
    // for protecting ctx, shared between threads
    std::mutex mtx;
    // the thread should be suspended while unwinding
    // blocked in the signal handler of target thread
    std::condition_variable cv;

    // register
    uintptr_t pc {0};
    uintptr_t sp {0};
    uintptr_t fp {0};
    uintptr_t lr {0};
    // stack
    uint8_t buffer[STACK_BUFFER_SIZE] {0};
    // other thread stack range
    uintptr_t stackBottom;
    uintptr_t stackTop;
};

struct UnwindContextMix {
    std::shared_ptr<ThreadContextSupportMix> context;
    std::shared_ptr<DfxMaps> maps;
};

class LocalThreadContextSupportMix {
public:
    static LocalThreadContextSupportMix& GetInstance();

    bool CollectThreadContext(int32_t tid,
        std::shared_ptr<ThreadContextSupportMix> ctx);
    void SetContext(std::shared_ptr<ThreadContextSupportMix> ctx);
    std::shared_ptr<ThreadContextSupportMix> GetContext();
    void SetTid(int32_t tid);
    int32_t GetTid();
    void SetStatus(int status);
    int GetStatus();

private:
    static void CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context);
    bool SignalRequestThread(int32_t tid);
    void InitSignalHandler();

private:
    std::mutex localMutex_;
    std::shared_ptr<ThreadContextSupportMix> context_;
    int32_t tid_ = -1;
    int status_ = SyncStatus::INIT;
};
} // namespace Dfx
} // namespace OHOS
#endif