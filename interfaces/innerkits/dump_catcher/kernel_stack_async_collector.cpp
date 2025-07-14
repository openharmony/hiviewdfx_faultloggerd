/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <future>
#include <thread>
#include <vector>
#include <unistd.h>

#include "dfx_kernel_stack.h"
#include "dfx_log.h"
#include "elapsed_time.h"
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

KernelStackAsyncCollector::KernelResult KernelStackAsyncCollector::GetProcessStackWithTimeout(int pid,
    uint32_t timeoutMs) const
{
    if (asyncCount_ > maxAsyncTaskNum_) {
        DFXLOGE("GetProcessStackWithTimeout fail, overlimit pid:%{public}d count:%{public}d",
            pid, static_cast<int>(asyncCount_));
        return KernelResult {STACK_OVER_LIMIT};
    }
    std::promise<KernelResult> result;
    auto f = result.get_future();
    // kernel may take much time
    std::thread {CollectKernelStackTask, pid, std::move(result)}.detach();
    auto st = f.wait_for(std::chrono::milliseconds(timeoutMs));
    if (st == std::future_status::timeout) {
        DFXLOGE("GetStackWithTimeout task timeout pid:%{public}d", pid);
        return KernelResult {STACK_TIMEOUT};
    } else if (st == std::future_status::deferred) {
        DFXLOGE("GetStackWithTimeout task deferred pid:%{public}d", pid);
        return KernelResult {STACK_DEFERRED};
    }
    return f.get();
}

bool KernelStackAsyncCollector::NotifyStartCollect(int pid)
{
    if (asyncCount_ > maxAsyncTaskNum_) {
        DFXLOGE("NotifyStartCollect fail, overlimit pid:%{public}d count:%{public}d",
            pid, static_cast<int>(asyncCount_));
        return false;
    }
    std::promise<KernelResult> result;
    stackFuture_ = result.get_future();
    // kernel may take much time
    std::thread {CollectKernelStackTask, pid, std::move(result)}.detach();
    return true;
}

KernelStackAsyncCollector::KernelResult KernelStackAsyncCollector::GetCollectedStackResult()
{
    if (!stackFuture_.valid()) {
        DFXLOGE("GetCollectStackResult fail, overlimit count:%{public}d", static_cast<int>(asyncCount_));
        return KernelResult {STACK_OVER_LIMIT};
    }
    auto st = stackFuture_.wait_for(std::chrono::milliseconds(0));
    if (st == std::future_status::timeout) {
        DFXLOGE("GetCollectStackResult task timeout");
        return KernelResult {STACK_TIMEOUT};
    } else if (st == std::future_status::deferred) {
        DFXLOGE("GetCollectStackResult task deferred");
        return KernelResult {STACK_DEFERRED};
    }
    return stackFuture_.get();
}

class AutoCounter {
public:
    explicit AutoCounter(std::atomic<int> &count) : count_(count)
    {
        count_++;
    }
    AutoCounter(const AutoCounter&) = delete;
    AutoCounter& operator=(const AutoCounter&) = delete;

    ~AutoCounter()
    {
        count_--;
    }
private:
    std::atomic<int> &count_;
};

void KernelStackAsyncCollector::CollectKernelStackTask(int pid, std::promise<KernelResult> result)
{
    AutoCounter autoCounter(asyncCount_);
    ElapsedTime timer;
    if (!CheckProcessValid(pid)) {
        DFXLOGW("No process(%{public}d) status file exist!", pid);
        result.set_value(KernelResult {STACK_NO_PROCESS});
        return;
    }
    std::string kernelStackInfo;
    int kernelRet = 0;
    std::function<bool(int)> stackTask = [&kernelStackInfo, &kernelRet](int tid) {
        if (tid <= 0) {
            return false;
        }
        std::string tidKernelStackInfo;
        int32_t ret = DfxGetKernelStack(tid, tidKernelStackInfo);
        if (ret == 0) {
            kernelStackInfo.append(tidKernelStackInfo);
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
        [kernelErr] (const KerrCodeToErrCode &kerrCodeToErrCode) { return kerrCodeToErrCode.kErrCode == kernelErr; });
    return iter != std::end(ERR_CODE_CONVERT_TABLE) ? iter->errCode : STACK_UNKNOWN;
}
} // namespace HiviewDFX
} // namespace OHOS
