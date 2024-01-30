/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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
#ifndef BACKTRACE_LOCAL_CONTEXT_H
#define BACKTRACE_LOCAL_CONTEXT_H

#include <cinttypes>
#include <csignal>
#include <mutex>

#include <libunwind.h>
#include <nocopyable.h>

#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
enum ThreadContextStatus {
    CONTEXT_UNUSED = -1,
    CONTEXT_READY = -2,
};

struct ThreadContext {
    std::atomic<int32_t> tid {ThreadContextStatus::CONTEXT_UNUSED};
    // for protecting ctx, shared between threads
    std::mutex lock;
    // the thread should be suspended while unwinding
    // we are blocked in the signal handler of target thread
    std::condition_variable cv;
    // store unwind context
    unw_context_t* ctx {nullptr};
    // unwind in signal handler
    uintptr_t pcs[DEFAULT_MAX_LOCAL_FRAME_NUM] {0};
    std::atomic<int32_t> frameSz {0};
    ~ThreadContext()
    {
        std::unique_lock<std::mutex> mlock(lock);
        if (ctx != nullptr) {
            delete ctx;
            ctx = nullptr;
        }
    };
};

class BacktraceLocalContext {
public:
    static BacktraceLocalContext& GetInstance();
    ~BacktraceLocalContext() = default;
    // ThreadContext is released after calling ReleaseThread
    std::shared_ptr<ThreadContext> CollectThreadContext(int32_t tid);
    std::shared_ptr<ThreadContext> GetThreadContext(int32_t tid);
    bool ReadUintptrSafe(uintptr_t addr, uintptr_t& value);
    void ReleaseThread(int32_t tid);
    void CleanUp();
private:
    BacktraceLocalContext() = default;
    DISALLOW_COPY_AND_MOVE(BacktraceLocalContext);
    static bool CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context);
    bool SignalRequestThread(int32_t tid, ThreadContext* ctx);
    bool Init();
private:
    bool init_ {false};
    int32_t pipe2_[PIPE_NUM_SZ];
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
