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
#include "fp_unwinder.h"

#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <securec.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "dfx_log.h"
#include "stack_util.h"

namespace OHOS {
namespace HiviewDFX {
static int32_t g_validPipe[PIPE_NUM_SZ] = {-1, -1};
constexpr uintptr_t MAX_UNWIND_ADDR_RANGE = 16 * 1024;
int32_t FpUnwinder::Unwind(uintptr_t* pcs, int32_t sz, int32_t skipFrameNum)
{
    GetFpPcRegs(pcs);
    int32_t index = 0;
    uintptr_t firstFp = pcs[1];
    uintptr_t fp = firstFp;
    uintptr_t stackPtr = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
    uintptr_t stackBottom = 0;
    uintptr_t stackTop = 0;
    uint32_t realSz = 0;
    if (getpid() == gettid()) {
        GetMainStackRange(stackBottom, stackTop);
    } else {
        GetSelfStackRange(stackBottom, stackTop);
        if (!(stackPtr >= stackBottom && stackPtr < stackTop)) {
            GetSigAltStackRange(stackBottom, stackTop);
            if (stackPtr < stackBottom || stackPtr >= stackTop) {
                return realSz;
            }
        }
    }
    while ((index < sz - 1) && (fp - firstFp < MAX_UNWIND_ADDR_RANGE)) {
        if (fp < stackBottom || fp >= stackTop - sizeof(uintptr_t)) {
            break;
        }
        if ((++index) >= skipFrameNum) {
            pcs[index - skipFrameNum] = *reinterpret_cast<uintptr_t*>(fp + sizeof(uintptr_t));
            realSz = static_cast<uint32_t>(index - skipFrameNum + 1);
        }
        uintptr_t nextFp = *reinterpret_cast<uintptr_t*>(fp);
        if (nextFp <= fp) {
            break;
        }
        fp = nextFp;
    }
    return realSz;
}

int32_t FpUnwinder::UnwindFallback(uintptr_t* pcs, int32_t sz, int32_t skipFrameNum)
{
    if (pipe2(g_validPipe, O_CLOEXEC | O_NONBLOCK) != 0) {
        LOGE("Failed to init pipe, errno(%d)", errno);
        return 0;
    }
    uintptr_t firstFp = pcs[1];
    uintptr_t fp = firstFp;
    int32_t index = 0;
    int32_t realSz = 0;
    while ((index < sz - 1) && (fp - firstFp < MAX_UNWIND_ADDR_RANGE)) {
        uintptr_t addr = fp + sizeof(uintptr_t);
        if (!ReadUintptrSafe(addr, pcs[++index])) {
            break;
        }
        if ((++index) >= skipFrameNum) {
            pcs[index - skipFrameNum] = 0;
            realSz = index - skipFrameNum + 1;
        }
        uintptr_t prevFp = fp;
        if (!ReadUintptrSafe(prevFp, fp)) {
            break;
        }
        if (fp <= prevFp) {
            break;
        }
    }
    close(g_validPipe[PIPE_WRITE]);
    close(g_validPipe[PIPE_READ]);
    return realSz;
}

NO_SANITIZE bool FpUnwinder::ReadUintptrSafe(uintptr_t addr, uintptr_t& value)
{
    if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_write, g_validPipe[PIPE_WRITE], addr, sizeof(uintptr_t))) == -1) {
        LOGE("%s", "Failed to write addr to pipe, it is a invalid addr");
        return false;
    }
    value = *reinterpret_cast<uintptr_t *>(addr);
    return true;
}
}
}
