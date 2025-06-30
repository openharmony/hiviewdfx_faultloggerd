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

#include "fault_coredump_service.h"

#include <fstream>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_trace.h"
#include "dfx_util.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_socket.h"
#include "fault_common_util.h"
#include "procinfo.h"
#include "proc_util.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr const char* const FAULTLOGGERD_SERVICE_TAG = "FAULT_COREDUMP_SERVICE";
bool CheckCoredumpUID(uint32_t callerUid)
{
    const uint32_t whitelist[] = {
        0, // rootUid
        1202, // dumpcatcherUid
        7005, // taskManagerUid
    };
    if (std::find(std::begin(whitelist), std::end(whitelist), callerUid) == std::end(whitelist)) {
        DFXLOGW("%{public}s :: CheckCoredumpUID :: Coredump Uid(%{public}d) is unexpectly.",
                FAULTLOGGERD_SERVICE_TAG, callerUid);
        return false;
    }
    return true;
}

int32_t SendCancelSignal(int32_t processDumpPid)
{
    siginfo_t si{0};
    si.si_signo = SIGTERM;
    si.si_errno = 0;
    si.si_code = SIGLEAK_STACK_COREDUMP;
#ifndef FAULTLOGGERD_TEST
    if (syscall(SYS_rt_sigqueueinfo, processDumpPid, si.si_signo, &si) != 0) {
        DFXLOGE("%{public}s :: Failed to SYS_rt_sigqueueinfo signal(%{public}d), errno(%{public}d).",
            FAULTLOGGERD_SERVICE_TAG, si.si_signo, errno);
        return ResponseCode::CORE_DUMP_NOPROC;
    }
#endif
    return ResponseCode::REQUEST_SUCCESS;
}

bool SendMsgToCoredumpClient(int32_t targetPid, int32_t responseCode, const std::string& fileName,
    RecorderProcessMap& coredumpRecorder)
{
    int32_t savedConnectionFd = -1;

    coredumpRecorder.GetCoredumpSocketId(targetPid, savedConnectionFd);
    if (savedConnectionFd < 0) {
        DFXLOGE("%{public}s :: Client sockFd has been crashed.", __func__);
        return false;
    }
    
    CoreDumpResult coredumpResult;
    if (strncpy_s(coredumpResult.fileName, sizeof(coredumpResult.fileName),
        fileName.c_str(), fileName.size()) != 0) {
        DFXLOGE("%{public}s :: strncpy failed.", __func__);
        return false;
    }
    coredumpResult.retCode = responseCode;

    if (SendMsgToSocket(savedConnectionFd, &coredumpResult, sizeof(coredumpResult))) {
        return true;
    }
    return false;
}

bool HandleCoredumpAndCleanup(int32_t targetPid, int32_t retCode, const std::string& fileName,
    RecorderProcessMap& coredumpRecorder)
{
    if (!SendMsgToCoredumpClient(targetPid, retCode, fileName, coredumpRecorder)) {
        DFXLOGE("Send message to client failed, %{public}s %{public}d", __func__, __LINE__);
    }

    if (!coredumpRecorder.ClearTargetPid(targetPid)) {
        DFXLOGE("Remove targetpid %{public}d failed, %{public}s %{public}d", targetPid, __func__, __LINE__);
        return false;
    }
    return true;
}
}

RecorderProcessMap& RecorderProcessMap::GetInstance()
{
    static RecorderProcessMap instance;
    return instance;
}

bool RecorderProcessMap::IsEmpty()
{
    return coredumpProcessMap.empty();
}

bool RecorderProcessMap::HasTargetPid(int32_t targetPid)
{
    return coredumpProcessMap.find(targetPid) != coredumpProcessMap.end();
}

void RecorderProcessMap::AddProcessMap(int32_t targetPid, const CoredumpProcessInfo& processInfo)
{
    coredumpProcessMap.emplace(
        targetPid,
        CoredumpProcessInfo(processInfo.processDumpPid, processInfo.coredumpSocketId,
                            processInfo.endTime, processInfo.cancelFlag)
    );
}

bool RecorderProcessMap::ClearTargetPid(int32_t targetPid)
{
    auto it = coredumpProcessMap.find(targetPid);
    if (it != coredumpProcessMap.end()) {
        close(it->second.coredumpSocketId);
        coredumpProcessMap.erase(it);
        return true;
    }
    return false;
}

bool RecorderProcessMap::SetCancelFlag(int32_t targetPid, bool flag)
{
    auto it = coredumpProcessMap.find(targetPid);
    if (it != coredumpProcessMap.end()) {
        it->second.cancelFlag = flag;
        return true;
    }
    return false;
}

bool RecorderProcessMap::SetProcessDumpPid(int32_t targetPid, int32_t processDumpPid)
{
    auto it = coredumpProcessMap.find(targetPid);
    if (it != coredumpProcessMap.end()) {
        it->second.processDumpPid = processDumpPid;
        return true;
    }
    return false;
}

bool RecorderProcessMap::GetCoredumpSocketId(int32_t targetPid, int32_t& coredumpSocketId)
{
    auto it = coredumpProcessMap.find(targetPid);
    if (it != coredumpProcessMap.end()) {
        coredumpSocketId = it->second.coredumpSocketId;
        return true;
    }
    return false;
}

bool RecorderProcessMap::GetProcessDumpPid(int32_t targetPid, int32_t& processDumpPid)
{
    auto it = coredumpProcessMap.find(targetPid);
    if (it != coredumpProcessMap.end()) {
        processDumpPid = it->second.processDumpPid;
        return true;
    }
    return false;
}

bool RecorderProcessMap::GetCancelFlag(int32_t targetPid, bool& flag)
{
    auto it = coredumpProcessMap.find(targetPid);
    if (it != coredumpProcessMap.end()) {
        flag = it->second.cancelFlag;
        return true;
    }
    return false;
}

#ifndef is_ohos_lite
bool CoredumpStatusService::HandleProcessDumpPid(int32_t targetPid, int32_t processDumpPid)
{
    if (processDumpPid <= 0 || targetPid <= 0) {
        return false;
    }

    if (!coredumpRecorder.HasTargetPid(targetPid)) {
        return false;
    }

    bool cancelFlag = false;
    if (coredumpRecorder.GetCancelFlag(targetPid, cancelFlag)) {
        if (!cancelFlag) {
            coredumpRecorder.SetProcessDumpPid(targetPid, processDumpPid);
            return true;
        }
    }

    if (SendCancelSignal(processDumpPid) != ResponseCode::REQUEST_SUCCESS) {
        return false;
    }

    int32_t retCode = ResponseCode::CORE_DUMP_CANCEL;
    std::string fileName = "";
    HandleCoredumpAndCleanup(targetPid, retCode, fileName, coredumpRecorder);
    return true;
}

int32_t CoredumpStatusService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const CoreDumpStatusData& requestData)
{
    DFX_TRACE_SCOPED("CoredumpStatusServiceOnRequest");
    DFXLOGI("Receive coredump status request for pid:%{public}d, status:%{public}d", requestData.pid,
            requestData.coredumpStatus);

    int32_t res = ResponseCode::REQUEST_SUCCESS;
    if (requestData.coredumpStatus == CoreDumpStatus::CORE_DUMP_START) {
        DFXLOGI("Processdump start %{public}s %{public}d", __func__, __LINE__);
        int32_t processDumpPid = requestData.processDumpPid;

        if (!HandleProcessDumpPid(requestData.pid, processDumpPid)) {
            DFXLOGE("Handle processDumpPid %{public}d failed", processDumpPid);
            res = ResponseCode::ABNORMAL_SERVICE;
            return res;
        }
    } else if (requestData.coredumpStatus == CoreDumpStatus::CORE_DUMP_END) {
        DFXLOGI("Processdump finish %{public}s %{public}d", __func__, __LINE__);

        int32_t targetPid = requestData.pid;
        char cfileName[256];
        if (strncpy_s(cfileName, sizeof(cfileName), requestData.fileName, sizeof(requestData.fileName)) != 0) {
            DFXLOGE("%{public}s :: strncpy failed.", __func__);
            return ResponseCode::DEFAULT_ERROR_CODE;
        }
        int32_t retCode = requestData.retCode;
        HandleCoredumpAndCleanup(targetPid, retCode, cfileName, coredumpRecorder);
    }

    SendMsgToSocket(connectionFd, &res, sizeof(res));
    return res;
}

int32_t CoredumpService::Filter(const std::string& socketName, const CoreDumpRequestData& requestData, uint32_t uid)
{
    if (requestData.pid <= 0 || socketName != SERVER_SOCKET_NAME || !CheckCoredumpUID(uid)) {
        DFXLOGE("%{public}s :: HandleCoreDumpRequest :: pid(%{public}d) or socketName(%{public}s) fail.",
            FAULTLOGGERD_SERVICE_TAG, requestData.pid, socketName.c_str());
        return ResponseCode::REQUEST_REJECT;
    }
    if (TempFileManager::CheckCrashFileRecord(requestData.pid)) {
        DFXLOGW("%{public}s :: pid(%{public}d) has been crashed, break.",
                FAULTLOGGERD_SERVICE_TAG, requestData.pid);
        return ResponseCode::CORE_PROCESS_CRASHED;
    }
    return ResponseCode::REQUEST_SUCCESS;
}

int32_t CoredumpService::DoCoredumpRequest(const std::string& socketName, int32_t connectionFd,
    const CoreDumpRequestData& requestData)
{
    struct ucred creds;
    if (!FaultCommonUtil::GetUcredByPeerCred(creds, connectionFd)) {
        DFXLOGE("Core dump pid(%{public}d) request failed to get cred.", requestData.pid);
        return ResponseCode::REQUEST_REJECT;
    }
    int32_t responseCode = Filter(socketName, requestData, creds.uid);
    if (responseCode != ResponseCode::REQUEST_SUCCESS) {
        return responseCode;
    }

    int32_t res = ResponseCode::REQUEST_SUCCESS;
    int32_t coredumpSocketId = dup(connectionFd);
    int32_t targetPid = requestData.pid;
    if (coredumpRecorder.HasTargetPid(targetPid)) {
        DFXLOGE("%{public}d is generating coredump, please do not repeat dump!", targetPid);
        res = ResponseCode::CORE_DUMP_REPEAT;
        SendMsgToSocket(coredumpSocketId, &res, sizeof(res));
        close(coredumpSocketId);
        return res;
    }
    
    siginfo_t si{0};
    si.si_signo = SIGLEAK_STACK;
    si.si_errno = 0;
    si.si_code = SIGLEAK_STACK_COREDUMP;
    si.si_pid = static_cast<int32_t>(creds.pid);
    if (auto ret = FaultCommonUtil::SendSignalToProcess(targetPid, si); ret != ResponseCode::REQUEST_SUCCESS) {
        SendMsgToSocket(coredumpSocketId, &ret, sizeof(ret));
        close(coredumpSocketId);
        return ret;
    }
    
    int32_t processDumpPid = -1;
    uint64_t endTime = requestData.endTime;
    coredumpRecorder.AddProcessMap(targetPid, {processDumpPid, coredumpSocketId, endTime, false});
    SendMsgToSocket(coredumpSocketId, &res, sizeof(res));

    auto removeTask = [targetPid, coredumpRecorderPtr = &coredumpRecorder]() {
        coredumpRecorderPtr->ClearTargetPid(targetPid);
    };

    constexpr int32_t minDelays = 60; // 60 : 60s
    int32_t delays = static_cast<int32_t>((endTime - GetAbsTimeMilliSeconds()) / 1000);
    delays = std::min(delays, minDelays);
    StartDelayTask(removeTask, delays);
    return res;
}

int32_t CoredumpService::CancelCoredumpRequest(int32_t connectionFd, const CoreDumpRequestData& requestData)
{
    int32_t targetPid = requestData.pid;
    int32_t res = ResponseCode::REQUEST_SUCCESS;
    if (!coredumpRecorder.HasTargetPid(targetPid)) {
        DFXLOGE("No need to cancel!");
        res = ResponseCode::DEFAULT_ERROR_CODE;
        SendMsgToSocket(connectionFd, &res, sizeof(res));
        return res;
    }

    int32_t processDumpPid = -1;
    if (coredumpRecorder.GetProcessDumpPid(targetPid, processDumpPid)) {
        if (processDumpPid == -1) {
            DFXLOGE("Can not get processdump pid!");
            coredumpRecorder.SetCancelFlag(targetPid, true);
        } else {
            DFXLOGI("processDumpPid get %{public}d %{public}d", processDumpPid, __LINE__);

            res = SendCancelSignal(processDumpPid);
            if (res != ResponseCode::REQUEST_SUCCESS) {
                return res;
            }
            int32_t retCode = ResponseCode::CORE_DUMP_CANCEL;
            std::string fileName = "";
            HandleCoredumpAndCleanup(targetPid, retCode, fileName, coredumpRecorder);
        }
    }

    SendMsgToSocket(connectionFd, &res, sizeof(res));
    return res;
}

int32_t CoredumpService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const CoreDumpRequestData& requestData)
{
    DFX_TRACE_SCOPED("CoredumpServiceOnRequest");
    DFXLOGI("Receive coredump request for pid:%{public}d, action:%{public}d.", requestData.pid,
            requestData.coredumpAction);

    int32_t res = ResponseCode::REQUEST_SUCCESS;
    if (requestData.coredumpAction == CoreDumpAction::DO_CORE_DUMP) {
        DFXLOGI("Do coredump %{public}s %{public}d", __func__, __LINE__);
        res = DoCoredumpRequest(socketName, connectionFd, requestData);
    } else if (requestData.coredumpAction == CoreDumpAction::CANCEL_CORE_DUMP) {
        DFXLOGI("Cancel coredump %{public}s %{public}d", __func__, __LINE__);
        res = CancelCoredumpRequest(connectionFd, requestData);
    }
    return res;
}

void CoredumpService::StartDelayTask(std::function<void()> workFunc, int32_t delayTime)
{
    auto delayTask = DelayTask::CreateInstance(workFunc, delayTime);
    FaultLoggerDaemon::GetEpollManager(EpollManagerType::MAIN_SERVER).AddListener(std::move(delayTask));
}
#endif
}
}