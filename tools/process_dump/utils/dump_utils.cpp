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

#include <algorithm>
#include <vector>
#include <pthread.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#if defined(HAS_LIB_SELINUX)
#include <selinux/selinux.h>
#endif

#include "dfx_frame_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_log.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#include "dfx_offline_parser.h"
#ifndef is_ohos_lite
#include "bundle_mgr_interface.h"
#include "bundle_mgr_proxy.h"
#include "if_system_ability_manager.h"
#include "iservice_registry.h"
#include "parameters.h"
#include "system_ability_definition.h"
#endif
namespace OHOS {
namespace HiviewDFX {
namespace {
const char *const BLOCK_CRASH_PROCESS = "faultloggerd.priv.block_crash_process.enabled";
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

void DumpUtils::GetThreadKernelStack(DfxThread& thread, bool needParseSymbols)
{
    std::string threadKernelStack;
    pid_t tid = thread.GetThreadInfo().nsTid;
    DfxThreadStack threadStack;
    DfxOfflineParser offlineParser;
    DfxOfflineParser* parserPtr = needParseSymbols ? &offlineParser : nullptr;
    if (DfxGetKernelStack(tid, threadKernelStack, true) == 0 &&
        FormatThreadKernelStack(threadKernelStack, threadStack, parserPtr)) {
        DFXLOGW("Failed to get tid(%{public}d) user stack, try kernel", tid);
        thread.SetParseSymbolNecessity(false);
        thread.SetFrames(threadStack.frames);
    }
    if (IsBetaVersion()) {
        DFXLOGI("%{public}s", threadKernelStack.c_str());
    }
}

static inline bool HasNul(long val)
{
    char* ch = reinterpret_cast<char*>(&val);
    for (size_t i = 0; i < sizeof(long); i++) {
        if (ch[i] == '\0') {
            return true;
        }
    }
    return false;
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
        if (HasNul(ret)) {
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

AppExecFwk::BundleInfo* GetBundleInfo()
{
    static bool isInit = false;
    static AppExecFwk::BundleInfo bundleInfo;
    if (!isInit) {
        auto bundleInstance = GetBundleManager();
        if (bundleInstance == nullptr) {
            DFXLOGE("bundleInstance is nullptr");
            return nullptr;
        }
        int32_t flag = static_cast<int32_t>(AppExecFwk::GetBundleInfoFlag::GET_BUNDLE_INFO_WITH_REQUESTED_PERMISSION);
        auto ret = bundleInstance->GetBundleInfoForSelf(flag, bundleInfo);
        if (ret != ERR_OK) {
            DFXLOGE("GetBundleInfoForSelf failed! ret = %{public}d", ret);
            return nullptr;
        }
        isInit = true;
    }
    return &bundleInfo;
}
#endif

std::string DumpUtils::GetSelfBundleName()
{
#ifndef is_ohos_lite
    auto bundleInfo = GetBundleInfo();
    if (bundleInfo == nullptr) {
        return "";
    }
    return bundleInfo->name;
#endif
    return "";
}

bool DumpUtils::HasCoredumpPermission()
{
#ifndef is_ohos_lite
    auto bundleInfo = GetBundleInfo();
    if (bundleInfo == nullptr) {
        return false;
    }
    const std::vector<std::string>& perms = bundleInfo->reqPermissions;
    return std::find(perms.begin(), perms.end(), "ohos.permission.ALLOW_COREDUMP") != perms.end();
#endif
    return false;
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

bool IsBlockCrashProcess()
{
#ifndef is_ohos_lite
    static bool isBlockCrash = OHOS::system::GetParameter(BLOCK_CRASH_PROCESS, "false") == "true";
    return isBlockCrash;
#else
    return false;
#endif
}

void DumpUtils::WaitForFork(pid_t pid, pid_t& childPid)
{
    DFXLOGI("start wait fork event happen");
    int waitStatus = 0;
    pid_t result = waitpid(pid, &waitStatus, 0); // wait fork event
    if (result != pid) {
        DFXLOGE("waitpid failed, expected pid:%{public}d, got:%{public}d", pid, result);
        return;
    }
    DFXLOGI("wait for fork status %{public}d", waitStatus);
    if (static_cast<unsigned int>(waitStatus) >> 8 == (SIGTRAP | (PTRACE_EVENT_FORK << 8))) { // 8 : get fork event
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &childPid);
        DFXLOGI("next child pid %{public}d", childPid);
        waitpid(childPid, &waitStatus, 0); // wait child stop event
    }
}

void DumpUtils::NotifyOperateResult(ProcessDumpRequest& request, int result)
{
    if (request.childPipeFd[0] != -1) {
        close(request.childPipeFd[0]);
        request.childPipeFd[0] = -1;
    }
    if (OHOS_TEMP_FAILURE_RETRY(write(request.childPipeFd[1], &result, sizeof(result))) < 0) {
        DFXLOGE("write to child process fail %{public}d", errno);
    }
    if (request.childPipeFd[1] != -1) {
        close(request.childPipeFd[1]);
        request.childPipeFd[1] = -1;
    }
}

void DumpUtils::BlockCrashProcExit(const ProcessDumpRequest& request)
{
    if (!IsBlockCrashProcess()) {
        return;
    }
    DFXLOGI("start block crash process pid %{public}d nspid %{public}d", request.pid, request.nsPid);
    if (ptrace(PTRACE_POKEDATA, request.nsPid, reinterpret_cast<void*>(request.blockCrashExitAddr),
        CRASH_BLOCK_EXIT_FLAG) < 0) {
        DFXLOGE("pok block falg to nsPid %{public}d fail %{public}s", request.nsPid, strerror(errno));
    }
}

bool DumpUtils::IsSelinuxPermissive()
{
#if defined(HAS_LIB_SELINUX)
    return security_getenforce() == 0;
#else
    return true;
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
