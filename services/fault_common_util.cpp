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

#include "fault_common_util.h"

#include <fstream>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#include "fault_logger_daemon.h"
#include "procinfo.h"
#include "proc_util.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr const char* const FAULTLOGGERD_SERVICE_TAG = "FAULT_COMMON_UTIL";
}

namespace FaultCommonUtil {
bool GetUcredByPeerCred(struct ucred& rcred, int32_t connectionFd)
{
    socklen_t credSize = sizeof(rcred);
    if (getsockopt(connectionFd, SOL_SOCKET, SO_PEERCRED, &rcred, &credSize) != 0) {
        DFXLOGE("%{public}s :: Failed to GetCredential, errno: %{public}d", FAULTLOGGERD_SERVICE_TAG, errno);
        return false;
    }
    return true;
}

int32_t SendSignalToHapWatchdogThread(pid_t pid, const siginfo_t& si)
{
    long uid = 0;
    uint64_t sigBlk = 0;
    if (!GetUidAndSigBlk(pid, uid, sigBlk)) {
        return ResponseCode::DEFAULT_ERROR_CODE;
    };
    constexpr long minUid = 10000; // 10000 : minimum uid for hap
    if (uid < minUid || IsSigDumpMask(sigBlk)) {
        return ResponseCode::DEFAULT_ERROR_CODE;
    }

    pid_t tid = GetTidByThreadName(pid, "OS_DfxWatchdog");
    if (tid <= 0) {
        return ResponseCode::DEFAULT_ERROR_CODE;
    }
#ifndef FAULTLOGGERD_TEST
    if (syscall(SYS_rt_tgsigqueueinfo, pid, tid, si.si_signo, &si) != 0) {
        DFXLOGE("Failed to SYS_rt_tgsigqueueinfo signal(%{public}d), errno(%{public}d).",
            si.si_signo, errno);
        return ResponseCode::SDK_DUMP_NOPROC;
    }
#endif
    return ResponseCode::REQUEST_SUCCESS;
}

int32_t SendSignalToProcess(pid_t pid, const siginfo_t& si)
{
    auto ret = SendSignalToHapWatchdogThread(pid, si);
    if (ret == ResponseCode::SDK_DUMP_NOPROC || ret == ResponseCode::REQUEST_SUCCESS) {
        return ret;
    }
#ifndef FAULTLOGGERD_TEST
    if (syscall(SYS_rt_sigqueueinfo, pid, si.si_signo, &si) != 0) {
        DFXLOGE("Failed to SYS_rt_sigqueueinfo signal(%{public}d), errno(%{public}d).",
            si.si_signo, errno);
        return ResponseCode::SDK_DUMP_NOPROC;
    }
#endif
    return ResponseCode::REQUEST_SUCCESS;
}
}
}
}
