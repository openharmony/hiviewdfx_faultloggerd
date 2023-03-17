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
#ifndef BACKTRACE_LOCAL_STATIC_H
#define BACKTRACE_LOCAL_STATIC_H

#include <cinttypes>
#include <csignal>
#include <mutex>

#include <libunwind.h>
#include <nocopyable.h>

namespace OHOS {
namespace HiviewDFX {
enum ThreadContextStatus {
    ContextUnused = -1,
    ContextUsing = -2,
    ContextReady = -3,
};
struct ThreadContext {
    std::atomic<int32_t> tid {ThreadContextStatus::ContextUnused};
    // for protecting ctx, shared between threads
    std::mutex lock;
    // the thread should be suspended while unwinding
    // we are blocked in the signal handler of target thread
    std::condition_variable cv;
    // store unwind context
    unw_context_t* ctx {nullptr};
    ~ThreadContext()
    {
        std::unique_lock<std::mutex> mlock(lock);
        if (ctx != nullptr) {
            delete ctx;
            ctx = nullptr;
        }
    };
};
class BacktraceLocalStatic {
public:
    static BacktraceLocalStatic& GetInstance();
    ~BacktraceLocalStatic() = default;
    // ThreadContext is released after calling ReleaseThread
    std::shared_ptr<ThreadContext> GetThreadContext(int32_t tid);
    void ReleaseThread(int32_t tid);
    void CleanUp();
private:
    BacktraceLocalStatic() = default;
    DISALLOW_COPY_AND_MOVE(BacktraceLocalStatic);
    static void CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context);
    bool InstallSigHandler();
    void UninstallSigHandler();
    bool SignalRequestThread(int32_t tid, ThreadContext* ctx);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // BACKTRACE_LOCAL_STATIC_H
