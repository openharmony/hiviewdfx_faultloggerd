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

#include "reporter.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <string>
#include "dfx_define.h"

#include "dfx_log.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "faultlogger_client_msg.h"
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif

const char* const FOUNDATION_PROCESS_NAME = "foundation";
const char* const HIVIEW_PROCESS_NAME = "/system/bin/hiview";
const char* const REGS_KEY_WORD = "Registers:\n";
#ifndef HISYSEVENT_DISABLE
const char* const KILL_REASON_CPP_CRASH = "Kill Reason:Cpp Crash";
#endif

using RecordAppExitReason = int (*)(int reason, const char *exitMsg);

namespace OHOS {
namespace HiviewDFX {
SysEventReporter::SysEventReporter(const ProcessDumpType& processDumpType)
{
    if (processDumpType == ProcessDumpType::DUMP_TYPE_CPP_CRASH) {
        reporter_ = std::make_shared<CppCrashReporter>();
    } else if (processDumpType >=  ProcessDumpType::DUMP_TYPE_FDSAN
        && processDumpType <=  ProcessDumpType::DUMP_TYPE_BADFD) {
        reporter_ = std::make_shared<AddrSanitizerReporter>();
    }
}
void SysEventReporter::Report(DfxProcess& process, const ProcessDumpRequest &request)
{
    if (reporter_ != nullptr) {
        reporter_->Report(process, request);
    }
}

void CppCrashReporter::Report(DfxProcess& process, const ProcessDumpRequest &request)
{
    if (process.GetProcessInfo().processName.find(HIVIEW_PROCESS_NAME) == std::string::npos) {
        ReportToHiview(process, request);
    } else {
        DFXLOGW("Do not to report to hiview, because hiview is crashed.");
    }
    if (process.GetProcessInfo().processName.find(FOUNDATION_PROCESS_NAME) == std::string::npos) {
        ReportToAbilityManagerService(process);
    } else {
        DFXLOGW("Do not to report to AbilityManagerService, because foundation is crashed.");
    }
}

void CppCrashReporter::ReportToHiview(DfxProcess& process, const ProcessDumpRequest &request)
{
    std::shared_ptr<void> handle(dlopen("libfaultlogger.z.so", RTLD_LAZY | RTLD_NODELETE), [] (void* handle) {
        if (handle != nullptr) {
            dlclose(handle);
        }
    });
    if (handle == nullptr) {
        DFXLOGW("Failed to dlopen libfaultlogger, %{public}s\n", dlerror());
        dlerror();
        return;
    }
    auto addFaultLog = reinterpret_cast<void (*)(FaultDFXLOGIInner*)>(dlsym(handle.get(), "AddFaultLog"));
    if (addFaultLog == nullptr) {
        DFXLOGW("Failed to dlsym AddFaultLog, %{public}s\n", dlerror());
        dlerror();
        return;
    }
    FaultDFXLOGIInner info;
    info.time = request.timeStamp;
    info.id = process.GetProcessInfo().uid;
    info.pid = process.GetProcessInfo().pid;
    SmartFd fdRead = TranferCrashInfoToHiview(process.GetCrashInfoJson());
    info.pipeFd = fdRead.GetFd(); // free after addFaultLog
    info.faultLogType = 2; // 2 : CPP_CRASH_TYPE
    info.logFileCutoffSizeBytes = process.GetCrashLogConfig().logFileCutoffSizeBytes;
    info.module = process.GetProcessInfo().processName;
    info.reason = process.GetReason();
    info.registers = GetRegsString(process.GetFaultThreadRegisters());
    info.summary = GetSummary(process);
    addFaultLog(&info);
    DFXLOGI("Finish report fault to FaultLogger %{public}s(%{public}d,%{public}d)",
        info.module.c_str(), info.pid, info.id);
}

std::string CppCrashReporter::GetSummary(DfxProcess& process)
{
    std::string summary;
    auto msg = process.GetFatalMessage();
    if (!msg.empty()) {
        summary += ("LastFatalMessage:" + msg + "\n");
    }
    if (process.GetKeyThread() == nullptr) {
        DFXLOGE("Failed to get key thread!");
        return summary;
    }
    bool needPrintTid = false;
    std::string threadInfo = process.GetKeyThread()->ToString(needPrintTid);
    auto iterator = threadInfo.begin();
    while (iterator != threadInfo.end() && *iterator != '\n') {
        if (isdigit(*iterator)) {
            iterator = threadInfo.erase(iterator);
        } else {
            iterator++;
        }
    }
    summary += threadInfo;
    return summary;
}

// read fd will be closed after transfering to hiview
SmartFd CppCrashReporter::TranferCrashInfoToHiview(const std::string& cppCrashInfo)
{
    size_t crashInfoSize = cppCrashInfo.size();
    if (crashInfoSize > MAX_PIPE_SIZE) {
        DFXLOGE("the size of json string is greater than max pipe size, do not report");
        return {};
    }
    int pipeFd[PIPE_NUM_SZ] = {-1, -1};
    if (pipe2(pipeFd, O_NONBLOCK) != 0) {
        DFXLOGE("Failed to create pipe.");
        return {};
    }
    SmartFd writeFd(pipeFd[PIPE_WRITE]);
    SmartFd readFd(pipeFd[PIPE_READ]);
    if (fcntl(readFd.GetFd(), F_SETPIPE_SZ, crashInfoSize) < 0 ||
        fcntl(writeFd.GetFd(), F_SETPIPE_SZ, crashInfoSize) < 0) {
        DFXLOGE("[%{public}d]: failed to set pipe size.", __LINE__);
        return {};
    }
    ssize_t realWriteSize = -1;
    realWriteSize = OHOS_TEMP_FAILURE_RETRY(write(writeFd.GetFd(), cppCrashInfo.c_str(), crashInfoSize));
    if (static_cast<ssize_t>(crashInfoSize) != realWriteSize) {
        DFXLOGE("Failed to write pipe. realWriteSize %{public}zd, json size %{public}zd", realWriteSize, crashInfoSize);
        return {};
    }
    return readFd;
}

void CppCrashReporter::ReportToAbilityManagerService(const DfxProcess& process)
{
    std::shared_ptr<void> handle(dlopen("libability_manager_c.z.so", RTLD_LAZY | RTLD_NODELETE), [] (void* handle) {
        if (handle != nullptr) {
            dlclose(handle);
        }
    });
    if (handle == nullptr) {
        DFXLOGW("Failed to dlopen libabilityms, %{public}s\n", dlerror());
        return;
    }

    RecordAppExitReason recordAppExitReason = (RecordAppExitReason)dlsym(handle.get(), "RecordAppExitReason");
    if (recordAppExitReason == nullptr) {
        DFXLOGW("Failed to dlsym RecordAppExitReason, %{public}s\n", dlerror());
        return;
    }

    // defined in interfaces/inner_api/ability_manager/include/ability_state.h
    const int cppCrashExitReason = 2;
    recordAppExitReason(cppCrashExitReason, process.GetReason().c_str());
#ifndef HISYSEVENT_DISABLE
    int result = HiSysEventWrite(HiSysEvent::Domain::FRAMEWORK, "PROCESS_KILL", HiSysEvent::EventType::FAULT,
        "PID", process.GetProcessInfo().pid, "PROCESS_NAME", process.GetProcessInfo().processName.c_str(),
        "MSG", KILL_REASON_CPP_CRASH);
    DFXLOGW("hisysevent write result=%{public}d, send event [FRAMEWORK,PROCESS_KILL], pid=%{public}d,"
        " processName=%{public}s, msg=%{public}s", result, process.GetProcessInfo().pid,
        process.GetProcessInfo().processName.c_str(), KILL_REASON_CPP_CRASH);
#endif
}

std::string CppCrashReporter::GetRegsString(std::shared_ptr<DfxRegs> regs)
{
    std::string regsString = "";
    if (regs == nullptr) {
        return regsString;
    }
    regsString = regs->PrintRegs();
    // if start with 'Registers:\n', need remove
    if (regsString.find(REGS_KEY_WORD) == 0) {
        regsString = regsString.substr(strlen(REGS_KEY_WORD));
    }
    return regsString;
}

void AddrSanitizerReporter::Report(DfxProcess& process, const ProcessDumpRequest &request)
{
#ifndef HISYSEVENT_DISABLE
    std::string reason = process.GetReason().empty() ? "DEBUG SIGNAL" : process.GetReason();
    std::string summary;
    if (process.GetKeyThread() != nullptr) {
        bool needPrintTid = false;
        summary = process.GetKeyThread()->ToString(needPrintTid);
    }
    HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::RELIABILITY, "ADDR_SANITIZER",
                    OHOS::HiviewDFX::HiSysEvent::EventType::FAULT,
                    "MODULE", request.processName,
                    "PID", request.pid,
                    "UID", request.uid,
                    "HAPPEN_TIME", request.timeStamp,
                    "REASON", reason,
                    "SUMMARY", summary);
    DFXLOGI("%{public}s", "Report ADDR_SANITIZER event done.");
#else
    DFXLOGW("%{public}s", "Not supported for ADDR_SANITIZER reporting.");
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
