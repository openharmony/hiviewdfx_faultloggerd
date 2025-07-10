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

#ifndef DFX_ASYNC_KERNEL_STACK_H
#define DFX_ASYNC_KERNEL_STACK_H

#include <atomic>
#include <cinttypes>
#include <future>
#include <string>
#include <utility>

namespace OHOS {
namespace HiviewDFX {

class KernelStackAsyncCollector {
public:
    enum ErrorCode : int32_t {
        STACK_SUCCESS = 0,
        STACK_ECREATE,
        STACK_EOPEN,
        STACK_EIOCTL,
        STACK_TIMEOUT,
        STACK_DEFERRED,
        STACK_OVER_LIMIT,
        STACK_NO_PROCESS,
        STACK_UNKNOWN,
    };
    struct KernelResult {
        ErrorCode errorCode = STACK_UNKNOWN;
        std::string msg = "";
        uint32_t threadCount = 0;
        KernelResult() = default;
        explicit KernelResult(ErrorCode errorCode) : errorCode(errorCode) {}
        KernelResult(ErrorCode errorCode, uint32_t threadCount) : errorCode(errorCode), threadCount(threadCount) {}
        KernelResult(ErrorCode errorCode, std::string msg, uint32_t threadCount)
            : errorCode(errorCode), msg(msg), threadCount(threadCount) {}
    };

    KernelResult GetProcessStackWithTimeout(int pid, uint32_t timeoutMs) const;

    bool NotifyStartCollect(int pid);
    KernelResult GetCollectedStackResult();
private:
    static void CollectKernelStackTask(int pid, std::promise<KernelResult> result);
    static bool CheckProcessValid(int pid);
    static ErrorCode ToErrCode(int kernelErr);

    static constexpr int maxAsyncTaskNum_ = 2;
    static std::atomic<int> asyncCount_;

    std::future<KernelResult> stackFuture_;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif