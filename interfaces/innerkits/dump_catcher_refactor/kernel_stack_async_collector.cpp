/*
 * Copyright (c) 2025-2026 Huawei Device Co., Ltd.
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

#include "kernel_stack_async_collector.h"
#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <unistd.h>

#include "dfx_kernel_stack.h"
#include "dfx_log.h"
#include "elapsed_time.h"
#include "proc_util.h"
#include "procinfo.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "AsyncKernelStack"
#endif

namespace OHOS {
namespace HiviewDFX {

std::atomic<int> KernelStackAsyncCollector::asyncCount_{0};

struct KerrCodeToErrCode {
    KernelStackErrorCode kErrCode;
    KernelStackAsyncCollector::ErrorCode errCode;
};

const struct KerrCodeToErrCode ERR_CODE_CONVERT_TABLE[] = {
    { KERNELSTACK_ESUCCESS, KernelStackAsyncCollector::STACK_SUCCESS },
    { KERNELSTACK_ECREATE, KernelStackAsyncCollector::STACK_ECREATE },
    { KERNELSTACK_EOPEN, KernelStackAsyncCollector::STACK_EOPEN },
    { KERNELSTACK_EIOCTL, KernelStackAsyncCollector::STACK_EIOCTL },
};

struct AsyncCollect {
    int pid;
    std::promise<KernelStackAsyncCollector::KernelResult> result;

    explicit AsyncCollect(int pid) : pid(pid) {}

    void SetPromise(std::promise<KernelStackAsyncCollector::KernelResult> prom)
    {
        result = std::move(prom);
    }
};

class AsyncCountGuard {
public:
    static AsyncCountGuard TryAcquire(int pid)
    {
        AsyncCountGuard guard;
        int expected = KernelStackAsyncCollector::asyncCount_.load();
        do {
            if (expected > KernelStackAsyncCollector::maxAsyncTaskNum_) {
                DFXLOGE("overlimit pid:%{public}d count:%{public}d", pid,
                    static_cast<int>(KernelStackAsyncCollector::asyncCount_.load()));
                return guard;
            }
        } while (!KernelStackAsyncCollector::asyncCount_.compare_exchange_weak(
            expected, expected + 1, std::memory_order_seq_cst, std::memory_order_relaxed));
        guard.owns_ = true;
        return guard;
    }

    static AsyncCountGuard AsThreadOwner()
    {
        AsyncCountGuard guard;
        guard.owns_ = true;
        return guard;
    }

    ~AsyncCountGuard()
    {
        if (owns_) {
            KernelStackAsyncCollector::asyncCount_--;
        }
    }

    bool Acquired() const { return owns_; }
    void Release() { owns_ = false; }

    AsyncCountGuard(const AsyncCountGuard&) = delete;
    AsyncCountGuard& operator=(const AsyncCountGuard&) = delete;
    AsyncCountGuard(AsyncCountGuard&& other) noexcept : owns_(other.owns_) { other.owns_ = false; }
    AsyncCountGuard& operator=(AsyncCountGuard&& other) noexcept
    {
        if (this != &other) {
            if (owns_) {
                KernelStackAsyncCollector::asyncCount_--;
            }
            owns_ = other.owns_;
            other.owns_ = false;
        }
        return *this;
    }

private:
    AsyncCountGuard() = default;
    bool owns_ = false;
};

void* KernelStackAsyncCollector::CollectKernelStackTaskWrapper(void *arg)
{
    if (arg == nullptr) {
        DFXLOGW("invalid arg!");
        return nullptr;
    }
    std::unique_ptr<AsyncCollect> collect(static_cast<AsyncCollect *>(arg));
    AsyncCountGuard guard = AsyncCountGuard::AsThreadOwner();
    CollectKernelStackTask(collect->pid, std::move(collect->result));
    return nullptr;
}

bool KernelStackAsyncCollector::CheckProcessValid(int pid)
{
    std::string statusPath = std::string {"/proc/"} + std::to_string(pid) + "/status";
    if (access(statusPath.c_str(), F_OK) != 0) {
        DFXLOGW("No process(%{public}d) status file exist!", pid);
        return false;
    }
    return true;
}

KernelStackAsyncCollector::ErrorCode KernelStackAsyncCollector::ToErrCode(int kernelErr)
{
    auto iter = std::find_if(std::begin(ERR_CODE_CONVERT_TABLE), std::end(ERR_CODE_CONVERT_TABLE),
        [kernelErr] (const KerrCodeToErrCode &item) { return item.kErrCode == kernelErr; });
    return iter != std::end(ERR_CODE_CONVERT_TABLE) ? iter->errCode : STACK_UNKNOWN;
}

KernelStackAsyncCollector::KernelResult KernelStackAsyncCollector::GetProcessStackWithTimeout(
    int pid, uint32_t timeoutMs) const
{
    AsyncCountGuard guard = AsyncCountGuard::TryAcquire(pid);
    if (!guard.Acquired()) {
        return KernelResult {STACK_OVER_LIMIT};
    }
    std::promise<KernelResult> result;
    auto future = result.get_future();
    auto collect = std::make_unique<AsyncCollect>(pid);
    collect->SetPromise(std::move(result));

    pthread_t tid = -1;
    int err = pthread_create(&tid, nullptr, CollectKernelStackTaskWrapper, collect.get());
    if (err != 0) {
        DFXLOGE("create thread failed, pid:%{public}d, error: %{public}s", pid, strerror(err));
        collect->result.set_value(KernelResult {STACK_RESOURCE_LIMIT});
        return KernelResult {STACK_RESOURCE_LIMIT};
    }
    collect.release();
    guard.Release();
    (void)pthread_detach(tid);
    auto st = future.wait_for(std::chrono::milliseconds(timeoutMs));
    if (st == std::future_status::timeout) {
        DFXLOGE("GetStackWithTimeout task timeout pid:%{public}d", pid);
        return KernelResult {STACK_TIMEOUT};
    }
    if (st == std::future_status::deferred) {
        DFXLOGE("GetStackWithTimeout task deferred pid:%{public}d", pid);
        return KernelResult {STACK_DEFERRED};
    }
    return future.get();
}

bool KernelStackAsyncCollector::NotifyStartCollect(int pid)
{
    AsyncCountGuard guard = AsyncCountGuard::TryAcquire(pid);
    if (!guard.Acquired()) {
        return false;
    }
    std::promise<KernelResult> result;
    stackFuture_ = result.get_future();
    auto collect = std::make_unique<AsyncCollect>(pid);
    collect->SetPromise(std::move(result));

    pthread_t tid = -1;
    int err = pthread_create(&tid, nullptr, CollectKernelStackTaskWrapper, collect.get());
    if (err != 0) {
        DFXLOGE("create thread failed, pid:%{public}d, error: %{public}s", pid, strerror(err));
        collect->result.set_value(KernelResult {STACK_RESOURCE_LIMIT});
        return false;
    }
    collect.release();
    guard.Release();
    (void)pthread_detach(tid);
    return true;
}

KernelStackAsyncCollector::KernelResult KernelStackAsyncCollector::GetCollectedStackResult()
{
    if (!stackFuture_.valid()) {
        DFXLOGE("GetCollectStackResult fail, overlimit count:%{public}d",
            static_cast<int>(asyncCount_.load()));
        return KernelResult {STACK_OVER_LIMIT};
    }
    auto st = stackFuture_.wait_for(std::chrono::milliseconds(0));
    if (st == std::future_status::timeout) {
        DFXLOGE("GetCollectStackResult task timeout");
        return KernelResult {STACK_TIMEOUT};
    }
    if (st == std::future_status::deferred) {
        DFXLOGE("GetCollectStackResult task deferred");
        return KernelResult {STACK_DEFERRED};
    }
    return stackFuture_.get();
}

void KernelStackAsyncCollector::CollectKernelStackTask(int pid, std::promise<KernelResult> result)
{
    AsyncCountGuard guard = AsyncCountGuard::AsThreadOwner();
    ElapsedTime timer;
    if (!CheckProcessValid(pid)) {
        DFXLOGW("No process(%{public}d) status file exist!", pid);
        result.set_value(KernelResult {STACK_NO_PROCESS});
        return;
    }
    std::string kernelStackInfo;
    int kernelRet = 0;
    bool isMainThread = true;
    auto stackTask = [pid, &kernelStackInfo, &kernelRet, &isMainThread](int tid) {
        if (tid <= 0) {
            return false;
        }
        ProcessInfo info;
        std::string threadStat;
        if (ParseProcInfo(pid, tid, info)) {
            threadStat = FomatProcessInfoToString(info) + "\n";
        }
        std::string tidKernelStackInfo;
        int32_t ret = DfxGetKernelStack(tid, tidKernelStackInfo, isMainThread);
        isMainThread = false;
        if (ret == 0) {
            kernelStackInfo.append(tidKernelStackInfo).append(threadStat);
        } else if (kernelRet == 0) {
            kernelRet = ret;
        }
        return true;
    };
    std::vector<int> tids;
    (void)GetTidsByPidWithFunc(pid, tids, stackTask);
    uint32_t threadCount = tids.size();
    if (kernelStackInfo.empty()) {
        DFXLOGE("Process(%{public}d) collect kernel stack fail!", pid);
        result.set_value({ToErrCode(kernelRet), threadCount});
        return;
    }
    result.set_value({STACK_SUCCESS, std::move(kernelStackInfo), threadCount});
    DFXLOGI("finish collect all tid info for pid(%{public}d) time(%{public}" PRId64 ")ms", pid,
        timer.Elapsed<std::chrono::milliseconds>());
}
} // namespace HiviewDFX
} // namespace OHOS
