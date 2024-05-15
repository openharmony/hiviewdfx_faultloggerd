/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "cppcrash_reporter.h"

#include <cinttypes>
#include <dlfcn.h>
#include <fcntl.h>
#include <map>
#include <sstream>
#include <string>
#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif

struct FaultLogInfoInner {
    uint64_t time {0};
    uint32_t id {0};
    int32_t pid {-1};
    int32_t pipeFd {-1};
    int32_t faultLogType {0};
    std::string module;
    std::string reason;
    std::string summary;
    std::string logPath;
    std::string registers;
    std::map<std::string, std::string> sectionMaps;
};
static const char FOUNDATION_PROCESS_NAME[] = "foundation";
static const char HIVIEW_PROCESS_NAME[] = "/system/bin/hiview";
static const char REGS_KEY_WORD[] = "Registers:\n";
#ifndef HISYSEVENT_DISABLE
    static const char KILL_REASON_CPP_CRASH[] = "Kill Reason:Cpp Crash";
#endif

using AddFaultLog = void (*)(FaultLogInfoInner* info);
using RecordAppExitReason = int (*)(int reason, const char *exitMsg);

namespace OHOS {
namespace HiviewDFX {

bool CppCrashReporter::Format()
{
    if (process_ == nullptr) {
        return false;
    }

    cmdline_ = process_->processInfo_.processName;
    pid_ = process_->processInfo_.pid;
    uid_ = process_->processInfo_.uid;
    reason_ = process_->reason;
    auto msg = process_->GetFatalMessage();
    if (!msg.empty()) {
        stack_ = "LastFatalMessage:" + msg + "\n";
    }

    if (process_->vmThread_ != nullptr) {
        std::string threadInfo = process_->vmThread_->ToString();
        auto iterator = threadInfo.begin();
        while (iterator != threadInfo.end() && *iterator != '\n') {
            if (isdigit(*iterator)) {
                iterator = threadInfo.erase(iterator);
            } else {
                iterator++;
            }
        }
        stack_ += threadInfo;

        // regs
        registers_ = GetRegsString(process_->vmThread_);
    }
    return true;
}

void CppCrashReporter::ReportToHiview()
{
    if (!Format()) {
        DFXLOG_WARN("%s", "Failed to format crash report.");
        return;
    }
    if (process_->processInfo_.processName.find(HIVIEW_PROCESS_NAME) != std::string::npos) {
        DFXLOG_WARN("%s", "Failed to report, hiview is crashed.");
        return;
    }

    void* handle = dlopen("libfaultlogger.z.so", RTLD_LAZY | RTLD_NODELETE);
    if (handle == nullptr) {
        DFXLOG_WARN("Failed to dlopen libfaultlogger, %s\n", dlerror());
        return;
    }

    AddFaultLog addFaultLog = (AddFaultLog)dlsym(handle, "AddFaultLog");
    if (addFaultLog == nullptr) {
        DFXLOG_WARN("Failed to dlsym AddFaultLog, %s\n", dlerror());
        dlclose(handle);
        return;
    }

    FaultLogInfoInner info;
    info.time = time_;
    info.id = uid_;
    info.pid = pid_;
    info.pipeFd = WriteCppCrashInfoByPipe();
    info.faultLogType = 2; // 2 : CPP_CRASH_TYPE
    info.module = cmdline_;
    info.reason = reason_;
    info.summary = stack_;
    info.registers = registers_;
    addFaultLog(&info);
    DFXLOG_INFO("Finish report fault to FaultLogger %s(%d,%d)", cmdline_.c_str(), pid_, uid_);
    dlclose(handle);
}

// read fd will be closed after transfering to hiview
int32_t CppCrashReporter::WriteCppCrashInfoByPipe()
{
    size_t sz = cppCrashInfo_.size();
    if (sz > MAX_PIPE_SIZE) {
        DFXLOG_ERROR("the size of json string is greater than max pipe size, do not report");
        return -1;
    }
    int pipeFd[2] = {-1, -1};
    if (pipe(pipeFd) != 0) {
        DFXLOG_ERROR("Failed to create pipe.");
        return -1;
    }
    if (fcntl(pipeFd[PIPE_READ], F_SETPIPE_SZ, sz) < 0 ||
        fcntl(pipeFd[PIPE_WRITE], F_SETPIPE_SZ, sz) < 0) {
        DFXLOG_ERROR("Failed to set pipe size.");
        return -1;
    }
    if (fcntl(pipeFd[PIPE_READ], F_GETFL) < 0) {
        DFXLOG_ERROR("Failed to set pipe size.");
        return -1;
    } else {
        uint32_t flags = static_cast<uint32_t>(fcntl(pipeFd[PIPE_READ], F_GETFL));
        flags |= O_NONBLOCK;
        if (fcntl(pipeFd[PIPE_READ], F_SETFL, flags) < 0) {
            DFXLOG_ERROR("Failed to set pipe flag.");
            return -1;
        }
    }
    ssize_t realWriteSize = -1;
    realWriteSize = OHOS_TEMP_FAILURE_RETRY(write(pipeFd[PIPE_WRITE], cppCrashInfo_.c_str(), sz));
    close(pipeFd[PIPE_WRITE]);
    if (static_cast<ssize_t>(cppCrashInfo_.size()) != realWriteSize) {
        DFXLOG_ERROR("Failed to write pipe. realWriteSize %zd, json size %zd", realWriteSize, sz);
        close(pipeFd[PIPE_READ]);
        return -1;
    }
    return pipeFd[PIPE_READ];
}

void CppCrashReporter::ReportToAbilityManagerService()
{
    if (process_->processInfo_.processName.find(FOUNDATION_PROCESS_NAME) != std::string::npos) {
        DFXLOG_WARN("%s", "Do not to report to AbilityManagerService, foundation is crashed.");
        return;
    }

    void* handle = dlopen("libability_manager_c.z.so", RTLD_LAZY | RTLD_NODELETE);
    if (handle == nullptr) {
        DFXLOG_WARN("Failed to dlopen libabilityms, %s\n", dlerror());
        return;
    }

    RecordAppExitReason recordAppExitReason = (RecordAppExitReason)dlsym(handle, "RecordAppExitReason");
    if (recordAppExitReason == nullptr) {
        DFXLOG_WARN("Failed to dlsym RecordAppExitReason, %s\n", dlerror());
        dlclose(handle);
        return;
    }

    // defined in interfaces/inner_api/ability_manager/include/ability_state.h
    const int cppCrashExitReason = 2;
    recordAppExitReason(cppCrashExitReason, reason_.c_str());
    dlclose(handle);
    #ifndef HISYSEVENT_DISABLE
        int result = HiSysEventWrite(HiSysEvent::Domain::FRAMEWORK, "PROCESS_KILL", HiSysEvent::EventType::FAULT,
            "PID", pid_, "PROCESS_NAME", cmdline_.c_str(), "MSG", KILL_REASON_CPP_CRASH);
        DFXLOG_INFO("hisysevent write result=%d, send event [FRAMEWORK,PROCESS_KILL], pid=%d processName=%s, msg=%s",
            result, pid_, cmdline_.c_str(), KILL_REASON_CPP_CRASH);
    #endif
}

std::string CppCrashReporter::GetRegsString(std::shared_ptr<DfxThread> thread)
{
    std::string regsString = "";
    if (thread == nullptr) {
        return regsString;
    }
    std::shared_ptr<DfxRegs> regs = thread->GetThreadRegs();
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
} // namespace HiviewDFX
} // namespace OHOS
