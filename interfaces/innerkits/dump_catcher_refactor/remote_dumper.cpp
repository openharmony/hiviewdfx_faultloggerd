/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "remote_dumper.h"

#include <cstdint>
#include <string>

#include "dfx_define.h"
#include "dfx_dump_catcher_errno.h"
#include "dfx_dump_catcher_slow_policy.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#include "dump_catcher_constants.h"
#include "dump_catcher_pipe_data.h"
#include "faultloggerd_client.h"
#include "kernel_stack_async_collector.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "RemoteDumper"
#endif

namespace OHOS {
namespace HiviewDFX {

int32_t RemoteDumper::DoDumpRemoteLocked(int pid, int tid, std::string& msg, bool isJson, int timeout)
{
    return DoDumpCatchRemote(pid, tid, msg, isJson, timeout);
}

int32_t RemoteDumper::DoDumpCatchRemote(int pid, int tid, std::string& msg, bool isJson, int timeout)
{
    DFX_TRACE_SCOPED_DLSYM("DoDumpCatchRemote");
    int32_t ret = DUMPCATCH_UNKNOWN;
    if (pid <= 0 || tid < 0 ||
        timeout <= DumpCatcherConstants::WAIT_KERNEL_STACK_TIMEOUT_MS) {
        msg.append("Result: pid(" + std::to_string(pid) + ") param error.\n");
        DFXLOGW("%{public}s :: %{public}s", __func__, msg.c_str());
        return DUMPCATCH_EPARAM;
    }
    if (timeoutAnalyzer_.IsFrozen(pid, ret) && ret == DUMPCATCH_TIMEOUT_KERNEL_FROZEN) {
        state_.stack = state_.stackKit.GetProcessStackWithTimeout(
            pid, DumpCatcherConstants::WAIT_KERNEL_STACK_TIMEOUT_MS);
        return ret;
    }
    if (DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(pid)) {
        DFXLOGW("dumpcatch in slow period, return pid (%{public}d) kernel stack directly!", pid);
        msg.append("Result: pid(" + std::to_string(pid) +
            ") last dump slow, return kernel stack directly.\n");
        state_.stack = state_.stackKit.GetProcessStackWithTimeout(
            pid, DumpCatcherConstants::WAIT_KERNEL_STACK_TIMEOUT_MS);
        return DUMPCATCH_TIMEOUT_DUMP_IN_SLOWPERIOD;
    }
    int pipeReadFd[] = { -1, -1 };
    uint64_t sdkDumpStartTime = GetAbsTimeMilliSeconds();
    int sdkdumpRet = RequestSdkDump(pid, tid, pipeReadFd, isJson, timeout);
    if (sdkdumpRet != ResponseCode::REQUEST_SUCCESS) {
        resultHandler_.DealWithSdkDumpRet(sdkdumpRet, pid, ret, msg);
        return ret;
    }
    DumpCatcherPipeData pipeData(pid, pipeReadFd[PIPE_BUF_INDEX], pipeReadFd[PIPE_RES_INDEX]);
    timeout -= static_cast<int>(GetAbsTimeMilliSeconds() - sdkDumpStartTime);
    int pollRet = DoDumpRemotePid(pid, msg, pipeData, isJson, timeout);
    resultHandler_.DealWithPollRet(pollRet, pid, ret, msg, pipeData);
    DFXLOGI("%{public}s :: pid(%{public}d) ret: %{public}d", __func__, pid, ret);
    return ret;
}

int RemoteDumper::DoDumpRemotePid(int pid, std::string& msg, DumpCatcherPipeData& pipeData,
    bool isJson, int32_t timeout)
{
    DFX_TRACE_SCOPED_DLSYM("DoDumpRemotePid");
    if (timeout <= 0) {
        DFXLOGW("timeout less than 0, try to get kernel stack and return directly!");
        state_.stack = state_.stackKit.GetProcessStackWithTimeout(
            pid, DumpCatcherConstants::WAIT_KERNEL_STACK_TIMEOUT_MS);
        return DUMP_POLL_TIMEOUT;
    }
    if (timeout < DumpCatcherConstants::SHORT_TIMEOUT_THRESHOLD_MS) {
        DFXLOGW("timeout less than 1 seconds, get kernel stack directly!");
        state_.notifyCollect = state_.stackKit.NotifyStartCollect(pid);
    }
    int ret = poller_.HandleReadPhase(pid, timeout, pipeData, isJson, msg);
    DFXLOGI("%{public}s :: pid(%{public}d) poll ret: %{public}d", __func__, pid, ret);
    return ret;
}

void RemoteDumper::ReportStats(int32_t pid, uint64_t requestTime, int32_t ret, void* retAddr)
{
    statsReporter_.ReportDumpCatcherStats(pid, requestTime, ret, retAddr);
}

} // namespace HiviewDFX
} // namespace OHOS
