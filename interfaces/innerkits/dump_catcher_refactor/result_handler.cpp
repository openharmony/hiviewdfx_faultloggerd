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

#include "result_handler.h"

#include <unistd.h>

#include "dfx_define.h"
#include "dfx_dump_catcher_errno.h"
#include "dfx_dump_catcher_slow_policy.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "dfx_util.h"
#include "dump_catcher_constants.h"
#include "file_ex.h"
#include "procinfo.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "ResultHandler"
#endif

namespace OHOS {
namespace HiviewDFX {

namespace {
bool IsLinuxKernel()
{
    static bool isLinux = [] {
        std::string content;
        LoadStringFromFile("/proc/version", content);
        if (content.empty()) {
            return true;
        }
        return content.find("Linux") != std::string::npos;
    }();
    return isLinux;
}
}

int32_t ResultHandler::KernelRet2DumpcatchRet(int32_t ret)
{
    switch (ret) {
        case KernelStackAsyncCollector::STACK_ECREATE:
            return DUMPCATCH_KERNELSTACK_ECREATE;
        case KernelStackAsyncCollector::STACK_EOPEN:
            return DUMPCATCH_KERNELSTACK_EOPEN;
        case KernelStackAsyncCollector::STACK_EIOCTL:
            return DUMPCATCH_KERNELSTACK_EIOCTL;
        case KernelStackAsyncCollector::STACK_TIMEOUT:
            return DUMPCATCH_KERNELSTACK_TIMEOUT;
        case KernelStackAsyncCollector::STACK_OVER_LIMIT:
            return DUMPCATCH_KERNELSTACK_OVER_LIMIT;
        case KernelStackAsyncCollector::STACK_RESOURCE_LIMIT:
            return DUMPCATCH_KERNELSTACK_RESOURCE_LIMIT;
        default:
            return DUMPCATCH_UNKNOWN;
    }
}

void ResultHandler::CollectKernelStackOnFail(int32_t uid, int pid)
{
    if (uid == DumpCatcherConstants::HIVIEW_UID || uid == DumpCatcherConstants::FOUNDATION_UID) {
        state_.stack = state_.stackKit.GetProcessStackWithTimeout(
            pid, DumpCatcherConstants::WAIT_KERNEL_STACK_TIMEOUT_MS);
    }
}

void ResultHandler::DealAfterPollFail(int pid, std::string& msg, DumpCatcherPipeData& pipeData)
{
    if (state_.notifyCollect) {
        state_.stack = state_.stackKit.GetCollectedStackResult();
    } else {
        state_.stack = state_.stackKit.GetProcessStackWithTimeout(
            pid, DumpCatcherConstants::WAIT_KERNEL_STACK_TIMEOUT_MS);
    }
    std::string halfProcStatus;
    std::string halfProcWchan;
    ReadProcessStatus(halfProcStatus, pid);
    if (IsLinuxKernel()) {
        ReadProcessWchan(halfProcWchan, pid, false, true);
    }
    msg.append(std::move(halfProcStatus));
    msg.append(std::move(halfProcWchan));
    if (!pipeData.GetMainThreadStackMsg().empty()) {
        state_.stack.msg.append("\nMain thread user stack (unsymbolized):\n");
        state_.stack.msg.append(pipeData.GetMainThreadStackMsg());
    }
}

void ResultHandler::HandlePollReturnCode(int pollRet, int pid, int32_t& ret,
    std::string& msg, bool& isPollFail)
{
    (void)pollRet;
    (void)pid;
    (void)isPollFail;
    if (msg.find("ptrace attach thread failed") != std::string::npos) {
        ret = DUMPCATCH_DUMP_EPTRACE;
    } else if (msg.find("stop unwinding") != std::string::npos) {
        ret = DUMPCATCH_DUMP_EUNWIND;
    } else if (msg.find("mapinfo is not exist") != std::string::npos) {
        ret = DUMPCATCH_DUMP_EMAP;
    } else {
        ret = DUMPCATCH_DUMP_ERROR;
    }
}

void ResultHandler::DealWithPollRet(int pollRet, int pid, int32_t& ret,
    std::string& msg, DumpCatcherPipeData& pipeData)
{
    bool isPollFail = true;
    switch (pollRet) {
        case DUMP_POLL_OK:
            if (pipeData.GetResMsg().find("the thread count of target process is over limit") !=
                std::string::npos) {
                ret = DUMPCATCH_THREAD_COUNT_OVERLIMIT;
                break;
            }
            ret = DUMPCATCH_ESUCCESS;
            isPollFail = false;
            break;
        case DUMP_POLL_NO_PARSE_SYMBOL:
            ret = DUMPCATCH_DUMP_ESYMBOL_NO_PARSE;
            isPollFail = false;
            break;
        case DUMP_POLL_PARSE_SYMBOL_TIMEOUT:
            ret = DUMPCATCH_DUMP_ESYMBOL_PARSE_TIMEOUT;
            isPollFail = false;
            break;
        case DUMP_POLL_FD:
            ret = DUMPCATCH_EFD;
            break;
        case DUMP_POLL_FAILED:
            ret = DUMPCATCH_EPOLL;
            break;
        case DUMP_POLL_TIMEOUT:
            timeoutAnalyzer_.AnalyzeTimeoutReason(pid, ret);
            if (ret == DUMPCATCH_TIMEOUT_DUMP_SLOW) {
                DfxDumpCatcherSlowPolicy::GetInstance().SetDumpCatcherSlowStat(pid);
            }
            break;
        case DUMP_POLL_RETURN:
            HandlePollReturnCode(pollRet, pid, ret, msg, isPollFail);
            break;
        default:
            ret = DUMPCATCH_UNKNOWN;
            break;
    }
    if (isPollFail) {
        DealAfterPollFail(pid, msg, pipeData);
    }
}

void ResultHandler::DealWithSdkDumpRet(int sdkdumpRet, int pid, int32_t& ret, std::string& msg)
{
    uint32_t uid = getuid();
    auto setKernelResult = [this, &ret, &msg, uid, pid](int32_t code, const std::string& text) {
        CollectKernelStackOnFail(static_cast<int32_t>(uid), pid);
        msg.append("Result: pid(" + std::to_string(pid) + ") " + text + ".\n");
        ret = code;
    };
    switch (sdkdumpRet) {
        case ResponseCode::SDK_DUMP_REPEAT:
            setKernelResult(DUMPCATCH_IS_DUMPING, "process is dumping");
            break;
        case ResponseCode::REQUEST_REJECT:
            msg.append("Result: pid(" + std::to_string(pid) + ") process check permission error.\n");
            ret = DUMPCATCH_EPERMISSION;
            break;
        case ResponseCode::SDK_DUMP_NOPROC:
            msg.append("Result: pid(" + std::to_string(pid) + ") process has exited.\n");
            ret = DUMPCATCH_NO_PROCESS;
            break;
        case ResponseCode::SDK_PROCESS_CRASHED:
            setKernelResult(DUMPCATCH_HAS_CRASHED, "process has been crashed");
            break;
        case ResponseCode::CONNECT_FAILED:
            setKernelResult(DUMPCATCH_ECONNECT, "process fail to conntect faultloggerd");
            break;
        case ResponseCode::SEND_DATA_FAILED:
            setKernelResult(DUMPCATCH_EWRITE, "process fail to write to faultloggerd");
            break;
        default:
            setKernelResult(DUMPCATCH_EFAULTLOGGERD, "faultloggerd maybe exception occurred");
            break;
    }
    DFXLOGW("%{public}s :: %{public}s", __func__, msg.c_str());
}

std::pair<int, std::string> ResultHandler::DealWithDumpCatchRet(int pid, int32_t& ret, std::string& msg)
{
    (void)pid;
    int result = ret == 0 ? 0 : -1;
    std::string reason;
    if (result == 0) {
        reason = "Reason:" + DfxDumpCatchError::ToString(ret) + "\n";
    } else if (ret == DUMPCATCH_DUMP_ESYMBOL_NO_PARSE ||
        ret == DUMPCATCH_DUMP_ESYMBOL_PARSE_TIMEOUT ||
        ret == DUMPCATCH_THREAD_COUNT_OVERLIMIT) {
        reason = "Reason:" + DfxDumpCatchError::ToString(ret) + "\n";
        result = 0;
    } else {
        reason = "Reason:\nnormal stack:" + DfxDumpCatchError::ToString(ret) + "\n";
        if (state_.stack.errorCode != KernelStackAsyncCollector::STACK_SUCCESS) {
            ret = KernelRet2DumpcatchRet(state_.stack.errorCode);
            reason += "kernel stack:" + DfxDumpCatchError::ToString(ret) + "\n";
        } else if (!state_.stack.msg.empty()) {
            msg.append(state_.stack.msg);
            result = 1;
        } else {
            reason += "kernel stack:" + DfxDumpCatchError::ToString(DUMPCATCH_KERNELSTACK_NONEED) + "\n";
        }
    }
    return std::make_pair(result, reason);
}

} // namespace HiviewDFX
} // namespace OHOS
