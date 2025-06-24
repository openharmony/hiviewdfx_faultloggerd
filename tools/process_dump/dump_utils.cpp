/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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
#include "dump_utils.h"

#include <vector>
#include <pthread.h>
#include "sys/ptrace.h"

#include "dfx_util.h"
#include "dfx_log.h"
#include "dfx_kernel_stack.h"
#include "dfx_frame_formatter.h"
#ifndef is_ohos_lite
#include "bundle_mgr_interface.h"
#include "bundle_mgr_proxy.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#endif
namespace OHOS {
namespace HiviewDFX {
namespace {
// defined in alltypes.h
#define DFX_MUTEX_TYPE u.i[0]
#define DFX_MUTEX_OWNER u.vi[1]
typedef struct {
    union {
        int i[sizeof(long) == 8 ? 10 : 6];
        volatile int vi[sizeof(long) == 8 ? 10 : 6];
        volatile void *volatile p[sizeof(long) == 8 ? 5 : 6];
    } u;
} DfxMutex;
}
bool DumpUtils::ParseLockInfo(Unwinder& unwinder, int32_t vmPid, int32_t tid)
{
#ifdef __aarch64__
    std::vector<char> buffer(sizeof(pthread_mutex_t), 0);
    if (unwinder.GetLockInfo(vmPid, buffer.data(), sizeof(pthread_mutex_t))) {
        auto mutexInfo = reinterpret_cast<DfxMutex*>(buffer.data());
        // only PTHREAD_MUTEX_ERRORCHECK(2) type lock contains lock holder thread-id
        // the normal type only store EBUSY in owner section
        int type = mutexInfo->DFX_MUTEX_TYPE;
        uint32_t owner = static_cast<uint32_t>(mutexInfo->DFX_MUTEX_OWNER) & 0x3fffffff;
        DFXLOGI("Thread(%{public}d) is waiting a lock with type %{public}d held by %{public}u", tid, type, owner);
        return true;
    }
    return false;
#else
    // not impl yet
    return false;
#endif
}

bool DumpUtils::IsLastValidFrame(const DfxFrame& frame)
{
    static uintptr_t libcStartPc = 0;
    static uintptr_t libffrtStartEntry = 0;
    if (((libcStartPc != 0) && (frame.pc == libcStartPc)) ||
        ((libffrtStartEntry != 0) && (frame.pc == libffrtStartEntry))) {
        return true;
    }

    if (frame.mapName.find("ld-musl-aarch64.so.1") != std::string::npos &&
        frame.funcName.find("start") != std::string::npos) {
        libcStartPc = frame.pc;
        return true;
    }

    if (frame.mapName.find("libffrt") != std::string::npos &&
        frame.funcName.find("CoStartEntry") != std::string::npos) {
        libffrtStartEntry = frame.pc;
        return true;
    }
    return false;
}

void DumpUtils::GetThreadKernelStack(DfxThread& thread)
{
    std::string threadKernelStack;
    pid_t tid = thread.GetThreadInfo().nsTid;
    DfxThreadStack threadStack;
    if (DfxGetKernelStack(tid, threadKernelStack) == 0 && FormatThreadKernelStack(threadKernelStack, threadStack)) {
        DFXLOGW("Failed to get tid(%{public}d) user stack, try kernel", tid);
        if (IsBetaVersion()) {
            DFXLOGI("%{public}s", threadKernelStack.c_str());
        }
        thread.SetParseSymbolNecessity(false);
        thread.SetFrames(threadStack.frames);
    }
}

std::string DumpUtils::ReadStringByPtrace(pid_t tid, uintptr_t startAddr, size_t maxLen)
{
    constexpr size_t maxReadLen = 1024 * 1024 * 1; // 1M
    if (maxLen == 0 || maxLen > maxReadLen) {
        DFXLOGE("maxLen(%{public}zu) is invalid value.", maxLen);
        return "";
    }
    size_t bufLen = (maxLen + sizeof(long) - 1) / sizeof(long);
    std::vector<long> buffer(bufLen, 0);
    for (size_t i = 0; i < bufLen; i++) {
        long ret = ptrace(PTRACE_PEEKTEXT, tid, reinterpret_cast<void*>(startAddr + sizeof(long) * i), nullptr);
        if (ret == -1) {
            DFXLOGE("read target mem by ptrace failed, errno(%{public}s).", strerror(errno));
            break;
        }
        buffer[i] = ret;
        if (ret == 0) {
            break;
        }
    }
    char* val = reinterpret_cast<char*>(buffer.data());
    val[maxLen - 1] = '\0';
    return std::string(val);
}

std::string DumpUtils::GetStackTrace(const std::vector<DfxFrame>& frames)
{
    std::string stackTrace;
    for (const auto& frame : frames) {
        stackTrace += DfxFrameFormatter::GetFrameStr(frame);
#if defined(__aarch64__)
        if (IsLastValidFrame(frame)) {
            break;
        }
#endif
    }
    return stackTrace;
}

bool DumpUtils::ReadTargetMemory(pid_t tid, uintptr_t addr, uintptr_t &value)
{
    uintptr_t targetAddr = addr;
    auto retAddr = reinterpret_cast<long*>(&value);
    for (size_t i = 0; i < sizeof(uintptr_t) / sizeof(long); i++) {
        *retAddr = ptrace(PTRACE_PEEKTEXT, tid, reinterpret_cast<void*>(targetAddr), nullptr);
        if (*retAddr == -1) {
            return false;
        }
        targetAddr += sizeof(long);
        retAddr += 1;
    }
    return true;
}

#ifndef is_ohos_lite
sptr<AppExecFwk::IBundleMgr> GetBundleManager()
{
    auto systemManager = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!systemManager) {
        DFXLOGE("Get system ability manager failed");
        return nullptr;
    }
    auto remoteObject = systemManager->GetSystemAbility(BUNDLE_MGR_SERVICE_SYS_ABILITY_ID);
    if (!remoteObject) {
        DFXLOGE("Get system ability failed");
        return nullptr;
    }
    sptr<AppExecFwk::IBundleMgr> bundleMgrProxy = iface_cast<AppExecFwk::IBundleMgr>(remoteObject);
    return bundleMgrProxy;
}
#endif

std::string DumpUtils::GetSelfBundleName()
{
#ifndef is_ohos_lite
    static std::string bundleName;
    if (!bundleName.empty()) {
        return bundleName;
    }
    auto bundleInstance = GetBundleManager();
    if (bundleInstance == nullptr) {
        DFXLOGE("bundleInstance is nullptr");
        return "";
    }
    AppExecFwk::BundleInfo bundleInfo;
    auto ret = bundleInstance->GetBundleInfoForSelf(0, bundleInfo);
    if (ret != ERR_OK) {
        DFXLOGE("GetBundleInfoForSelf failed! ret = %{public}d", ret);
        return "";
    }
    bundleName = bundleInfo.name;
    return bundleName;
#endif
    return "";
}

void DumpUtils::InfoCrashUnwindResult(const ProcessDumpRequest& request, bool isUnwindSucc)
{
    if (!isUnwindSucc) {
        return;
    }
    if (ptrace(PTRACE_POKEDATA, request.nsPid, reinterpret_cast<void*>(request.unwindResultAddr),
        CRASH_UNWIND_SUCCESS_FLAG) < 0) {
        DFXLOGE("pok unwind success flag to nsPid %{public}d fail %{public}s", request.nsPid, strerror(errno));
    }
}
} // namespace HiviewDFX
} // namespace OHOS
