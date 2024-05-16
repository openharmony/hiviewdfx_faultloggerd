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

#include <atomic>
#include <cinttypes>
#include <csignal>
#include <dlfcn.h>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <nocopyable.h>
#include <securec.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "dfx_ark.h"
#include "dfx_define.h"
#include "stack_util.h"

namespace OHOS {
namespace HiviewDFX {
class FpUnwinder {
public:
    static FpUnwinder* GetPtr()
    {
        static std::unique_ptr<FpUnwinder> ptr = nullptr;
        if (ptr == nullptr) {
            static std::once_flag flag;
            std::call_once(flag, [&] {
                ptr.reset(new FpUnwinder());
            });
        }
        return ptr.get();
    }
    ~FpUnwinder()
    {
        if (initPipe_) {
            close(pfd_[PIPE_WRITE]);
            close(pfd_[PIPE_READ]);
            initPipe_ = false;
        }
    }

    size_t Unwind(uintptr_t pc, uintptr_t fp, uintptr_t* pcs, size_t maxSize, size_t skipFrameNum = 0)
    {
        if (pcs == nullptr) {
            return 0;
        }
        if (gettid() == getpid()) {
            if (!GetMainStackRange(stackBottom_, stackTop_)) {
                return 0;
            }
        } else {
            if (!GetSelfStackRange(stackBottom_, stackTop_)) {
                return 0;
            }
        }
        uintptr_t firstFp = fp;
        size_t index = 0;
        MAYBE_UNUSED uintptr_t sp = 0;
        MAYBE_UNUSED bool isJsFrame = false;
        while ((index < maxSize) && (fp - firstFp < maxUnwindAddrRange_)) {
            if (index >= skipFrameNum) {
                pcs[index - skipFrameNum] = pc;
            }
            index++;
            uintptr_t prevFp = fp;
            if ((!ReadUptr(prevFp, fp)) ||
                (!ReadUptr(prevFp + sizeof(uintptr_t), pc))) {
                break;
            }
            if (fp <= prevFp || fp == 0) {
                break;
            }
        }
        return (index - skipFrameNum);
    }

    size_t UnwindSafe(uintptr_t pc, uintptr_t fp, uintptr_t* pcs, size_t maxSize, size_t skipFrameNum = 0)
    {
        if (pcs == nullptr) {
            return 0;
        }
        size_t index = 0;
        MAYBE_UNUSED uintptr_t sp = 0;
        MAYBE_UNUSED bool isJsFrame = false;
        while (index < maxSize) {
            if (index >= skipFrameNum) {
                pcs[index - skipFrameNum] = pc;
            }
            index++;
            uintptr_t prevFp = fp;
            if ((!ReadUptrSafe(prevFp, fp)) ||
                (!ReadUptrSafe(prevFp + sizeof(uintptr_t), pc))) {
                break;
            }
            if (fp <= prevFp || fp == 0) {
                break;
            }
        }
        return (index - skipFrameNum);
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

private:
    FpUnwinder()
    {
        if (pipe2(pfd_, O_CLOEXEC | O_NONBLOCK) == 0) {
            initPipe_ = true;
        }
    }
    DISALLOW_COPY_AND_MOVE(FpUnwinder);

#if defined(__has_feature) && __has_feature(address_sanitizer)
    __attribute__((no_sanitize("address"))) bool ReadUptr(uintptr_t addr, uintptr_t& value)
#else
    bool ReadUptr(uintptr_t addr, uintptr_t& value)
#endif
    {
        if ((addr < stackBottom_) || (addr + sizeof(uintptr_t) >= stackTop_)) {
            return false;
        }
        value = *reinterpret_cast<uintptr_t *>(addr);
        return true;
    }

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

        uintptr_t dummy;
        value = *reinterpret_cast<uintptr_t *>(addr);
        return OHOS_TEMP_FAILURE_RETRY(syscall(SYS_read, pfd_[PIPE_READ], &dummy, sizeof(uintptr_t))) ==
            sizeof(uintptr_t);
    }

private:
    MAYBE_UNUSED const uintptr_t maxUnwindAddrRange_ = 16 * 1024; // 16: 16k
    uintptr_t stackBottom_ = 0;
    uintptr_t stackTop_ = 0;
    MAYBE_UNUSED uintptr_t arkMapStart_ = 0;
    MAYBE_UNUSED uintptr_t arkMapEnd_ = 0;
    int32_t pfd_[PIPE_NUM_SZ] = {-1, -1};
    bool initPipe_ = false;
};
}
} // namespace OHOS
#endif