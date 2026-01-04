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
#include "coredump_signal_service.h"

#include <sys/syscall.h>

#include "dfx_log.h"
#include "dfx_define.h"
#include "dfx_socket_request.h"
#include "faultloggerd_socket.h"
#include "proc_util.h"
#include "procinfo.h"
#include "fault_common_util.h"

namespace OHOS {
namespace HiviewDFX {

int32_t CoredumpSignalService::SendStartSignal(pid_t targetPid)
{
    siginfo_t si{0};
    si.si_signo = SIGLEAK_STACK;
    si.si_errno = 0;
    si.si_code = -SIGLEAK_STACK_COREDUMP;
    if (auto ret = FaultCommonUtil::SendSignalToProcess(targetPid, si); ret != ResponseCode::REQUEST_SUCCESS) {
        return ret;
    }
    return ResponseCode::REQUEST_SUCCESS;
}

int32_t CoredumpSignalService::SendCancelSignal(pid_t workerPid)
{
    siginfo_t si{0};
    si.si_signo = SIGTERM;
    si.si_errno = 0;
#ifndef FAULTLOGGERD_TEST
    if (syscall(SYS_rt_sigqueueinfo, workerPid, si.si_signo, &si) != 0) {
        DFXLOGE("Failed to SYS_rt_sigqueueinfo signal(%{public}d), errno(%{public}d).",
            si.si_signo, errno);
        return ResponseCode::CORE_DUMP_NOPROC;
    }
#endif
    return ResponseCode::REQUEST_SUCCESS;
}
}
}
