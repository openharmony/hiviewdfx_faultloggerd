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
#ifndef FP_UNWINDER_H
#define FP_UNWINDER_H

#include <cinttypes>
#include <csignal>
#include <nocopyable.h>
#include <fcntl.h>
#include <securec.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "dfx_define.h"
#include "stack_util.h"

namespace OHOS {
namespace HiviewDFX {
class FpUnwinder {
public:
    static FpUnwinder& GetInstance()
    {
        static FpUnwinder instance;
        return instance;
    }
    ~FpUnwinder() = default;

    size_t Unwind(uintptr_t pc, uintptr_t fp, uintptr_t* pcs, size_t maxSize, size_t skipFrameNum = 0)
    {
        if (pcs == nullptr) {
            return 0;
        }
        uintptr_t stackBottom = 0;
        uintptr_t stackTop = 0;
        if (gettid() == getpid()) {
            if (!GetMainStackRange(stackBottom, stackTop)) {
                return 0;
            }
        } else {
            if (!GetSelfStackRange(stackBottom, stackTop)) {
                return 0;
            }
        }
        uintptr_t firstFp = fp;
        uintptr_t stepFp = fp;
        pcs[0] = pc;
        size_t index = 1;
        while ((index < maxSize) && (stepFp - firstFp < maxUnwindAddrRange_)) {
            if ((stepFp < stackBottom) || (stepFp >= stackTop - sizeof(uintptr_t))) {
                break;
            }
            uintptr_t prevFp = stepFp;
            if (index >= skipFrameNum) {
                pcs[index - skipFrameNum] = *reinterpret_cast<uintptr_t *>(prevFp + sizeof(uintptr_t));
            }
            stepFp = *reinterpret_cast<uintptr_t *>(prevFp);
            if (stepFp <= prevFp || stepFp == 0) {
                break;
            }
            index++;
        }
        return (index - skipFrameNum + 1);
    }

    size_t UnwindFallback(uintptr_t pc, uintptr_t fp, uintptr_t* pcs, size_t maxSize, size_t skipFrameNum = 0)
    {
        if (pcs == nullptr) {
            return 0;
        }
        uintptr_t stepFp = fp;
        pcs[0] = pc;
        size_t index = 1;
        while (index < maxSize) {
            uintptr_t prevFp = stepFp;
            if (index >= skipFrameNum) {
                if (!ReadUptrSafe(prevFp + sizeof(uintptr_t), pcs[index - skipFrameNum])) {
                    break;
                }
            }
            if (!ReadUptrSafe(prevFp, stepFp)) {
                break;
            }
            if (stepFp <= prevFp || stepFp == 0) {
                break;
            }
            index++;
        }
        return (index - skipFrameNum + 1);
    }

    static inline AT_ALWAYS_INLINE void GetPcFpRegs(void *regs)
    {
#if defined(__aarch64__)
        asm volatile(
        "1:\n"
        "adr x12, 1b\n"
        "stp x12, x29, [%[base], #0]\n"
        : [base] "+r"(regs)
        :
        : "x12", "memory");
#endif
    }

    inline bool InitPipe()
    {
        static std::once_flag flag;
        std::call_once(flag, [&]() {
            if (!initPipe_ && pipe2(pfd_, O_CLOEXEC | O_NONBLOCK) == 0) {
                initPipe_ = true;
            }
        });
        return initPipe_;
    }

    inline void ClosePipe()
    {
        if (initPipe_) {
            close(pfd_[PIPE_WRITE]);
            close(pfd_[PIPE_READ]);
            initPipe_ = false;
        }
    }

private:
    FpUnwinder() = default;
    DISALLOW_COPY_AND_MOVE(FpUnwinder);

#if defined(__has_feature) && __has_feature(address_sanitizer)
    __attribute__((no_sanitize("address"))) bool ReadUptrSafe(uintptr_t addr, uintptr_t& value)
#else
    bool ReadUptrSafe(uintptr_t addr, uintptr_t& value)
#endif
    {
        if (pfd_[PIPE_WRITE] < 0) {
            return false;
        }
        if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_write, pfd_[PIPE_WRITE], addr, sizeof(uintptr_t))) == -1) {
            return false;
        }
        value = *reinterpret_cast<uintptr_t *>(addr);
        return true;
    }

private:
    MAYBE_UNUSED const uintptr_t maxUnwindAddrRange_ = 16 * 1024; // 16: 16k
    int32_t pfd_[PIPE_NUM_SZ] = {-1, -1};
    bool initPipe_ = false;
};
}
} // namespace OHOS
#endif