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
#include <fstream>
#include <pthread.h>
#include "dfx_log.h"
namespace OHOS {
namespace HiviewDFX {
static int32_t g_validPipe[PIPE_NUM_SZ] = {-1, -1};
static thread_local uintptr_t g_mainThreadStackBottom = 0;
static thread_local uintptr_t g_mainThreadStackTop = 0;
constexpr uintptr_t g_maxUnwindAddrRange = 16 * 1024;
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
    if (getprocpid() == getproctid()) {
        GetMainThreadStackRange();
        stackBottom = g_mainThreadStackBottom;
        stackTop = g_mainThreadStackTop;
    } else {
        GetStackRange(stackBottom, stackTop);
        if (!(stackPtr >= stackBottom && stackPtr < stackTop)) {
            GetSignalAltStackRange(stackBottom, stackTop);
            if (stackPtr < stackBottom || stackPtr >= stackTop) {
                realSz = UnwindFallback(pcs, sz, skipFrameNum);
                return realSz;
            }
        }
    }
    while ((index < sz - 1) && (fp - firstFp < g_maxUnwindAddrRange)) {
        if (fp < stackBottom || fp >= stackTop - sizeof(uintptr_t)) {
            break;
        }
        if ((++index) >= skipFrameNum) {
            pcs[index - skipFrameNum] = *reinterpret_cast<uintptr_t*>(fp + sizeof(uintptr_t));
            realSz = index - skipFrameNum + 1;
        }
        uintptr_t nextFp = *reinterpret_cast<uintptr_t*>(fp);
        if (nextFp <= fp) {
            break;
        }
        fp = nextFp;
    }
    // try to unwind one more time using pipe validate addr
    if (realSz < 1) {
        realSz = UnwindFallback(pcs, sz, skipFrameNum);
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
    while ((index < sz - 1) && (fp - firstFp < g_maxUnwindAddrRange)) {
        uintptr_t addr = fp + sizeof(uintptr_t);
        uintptr_t tempAddr = 0;
        if (!ReadUintptrSafe(addr, pcs[++index])) {
            break;
        }
        if ((++index) >= skipFrameNum) {
            pcs[index - skipFrameNum] = tempAddr;
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

#if defined(__has_feature) && __has_feature(address_sanitizer)
__attribute__((no_sanitize("address"))) bool FpUnwinder::ReadUintptrSafe(uintptr_t addr, uintptr_t& value)
#else
bool FpUnwinder::ReadUintptrSafe(uintptr_t addr, uintptr_t& value)
#endif
{
    if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_write, g_validPipe[PIPE_WRITE], addr, sizeof(uintptr_t))) == -1) {
        LOGE("%s", "Failed to write addr to pipe, it is a invalid addr");
        return false;
    }
    value = *reinterpret_cast<uintptr_t *>(addr);
    return true;
}

void FpUnwinder::GetMainThreadStackRange()
{
    if (g_mainThreadStackBottom != 0 && g_mainThreadStackTop != 0) {
        return;
    }
    std::ifstream ifs;
    ifs.open("/proc/self/maps", std::ios::in);
    if (ifs.fail()) {
        LOGE("%s", "open proc/self/maps failed!");
        return;
    }
    std::string mapBuf;
    while (getline(ifs, mapBuf)) {
        if (mapBuf.find("[stack]") == std::string::npos) {
            continue;
        }
        uint32_t pos = 0;
        uint64_t begin = 0;
        uint64_t end = 0;
        uint64_t offset = 0;
        uint64_t major = 0;
        uint64_t minor = 0;
        ino_t inode = 0;
        char perms[5] = {0}; //5:rwxp
        std::string name;
        if (sscanf_s(mapBuf.c_str(), "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %x:%x %" SCNxPTR " %n",
            &begin, &end, &perms, sizeof(perms), &offset, &major, &minor, &inode, &pos) != 7) { // 7:scan size
            continue;
        }
        g_mainThreadStackBottom = begin;
        g_mainThreadStackTop = end;
        break;
    }
    ifs.close();
}

void FpUnwinder::GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    pthread_attr_t attr;
    void *base = nullptr;
    size_t size = 0;
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        if (pthread_attr_getstack(&attr, &base, &size) == 0) {
            stackBottom = reinterpret_cast<uintptr_t>(base);
            stackTop = reinterpret_cast<uintptr_t>(base) + size;
        }
    }
    pthread_attr_destroy(&attr);
}

void FpUnwinder::GetSignalAltStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    stack_t altStack;
    if (sigaltstack(nullptr, &altStack) != -1) {
        if ((altStack.ss_flags & SS_ONSTACK) != 0) {
            stackBottom = reinterpret_cast<uintptr_t>(altStack.ss_sp);
            stackTop = reinterpret_cast<uintptr_t>(altStack.ss_sp) + altStack.ss_size;
        }
    }
}
}
}