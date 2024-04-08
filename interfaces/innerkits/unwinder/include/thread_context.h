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

#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <nocopyable.h>
#include <string>

#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
enum ThreadContextStatus : int32_t {
    CONTEXT_UNUSED = -1,
    CONTEXT_READY = -2,
};

struct ThreadContext {
    std::atomic<int32_t> tid {ThreadContextStatus::CONTEXT_UNUSED};
    // for protecting ctx, shared between threads
    std::mutex mtx;
    // the thread should be suspended while unwinding
    // blocked in the signal handler of target thread
    std::condition_variable cv;
    // store unwind context
    ucontext_t* ctx {nullptr};
    // stack range
    uintptr_t stackBottom;
    uintptr_t stackTop;
#if defined(__aarch64__)
    // unwind in signal handler by fp
    uintptr_t pcs[DEFAULT_MAX_LOCAL_FRAME_NUM] {0};
#endif
    std::atomic<size_t> frameSz {0};

    ~ThreadContext()
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (ctx != nullptr) {
            delete ctx;
            ctx = nullptr;
        }
    };
};

class LocalThreadContext {
public:
    static LocalThreadContext& GetInstance();
    ~LocalThreadContext();

    bool GetStackRange(int32_t tid, uintptr_t& stackBottom, uintptr_t& stackTop);
    std::shared_ptr<ThreadContext> CollectThreadContext(int32_t tid);
    std::shared_ptr<ThreadContext> GetThreadContext(int32_t tid);
    void ReleaseThread(int32_t tid);
    void CleanUp();

private:
    LocalThreadContext() = default;
    DISALLOW_COPY_AND_MOVE(LocalThreadContext);

    static bool CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context);
    bool SignalRequestThread(int32_t tid, ThreadContext* ctx);
    void InitSignalHandler();

private:
    std::mutex localMutex_;
};
} // namespace Dfx
} // namespace OHOS
#endif
