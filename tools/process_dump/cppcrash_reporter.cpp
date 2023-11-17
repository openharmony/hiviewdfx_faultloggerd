/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include <map>
#include <sstream>
#include <string>
#include "dfx_logger.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_thread.h"

struct FaultLogInfoInner {
    uint64_t time {0};
    uint32_t id {0};
    int32_t pid {-1};
    int32_t faultLogType {0};
    std::string module;
    std::string reason;
    std::string summary;
    std::string logPath;
    std::string registers;
    std::string otherThreadInfo;
    std::map<std::string, std::string> sectionMaps;
};
static const char FOUNDATION_PROCESS_NAME[] = "foundation";
static const char HIVIEW_PROCESS_NAME[] = "/system/bin/hiview";
static const char REGS_KEY_WORD[] = "Registers:\n";

using AddFaultLog = void (*)(FaultLogInfoInner* info);
using RecordAppExitReason = int (*)(int reason);

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

        // other thread info
        std::vector<std::shared_ptr<DfxThread>> otherThreads = process_->GetOtherThreads();
        unsigned long index = 0;
        for (auto oneThread : otherThreads) {
            std::stringstream ss;
            ss << "Tid:" << oneThread->threadInfo_.tid << ", ";
            otherThreadInfo_ += ss.str() + oneThread->ToString();
            index++;
        }
    }
    return true;
}

void CppCrashReporter::ReportToHiview()
{
    if (!Format()) {
        DFXLOG_WARN("Failed to format crash report.");
        return;
    }
    if (process_->processInfo_.processName.find(HIVIEW_PROCESS_NAME) != std::string::npos) {
        DFXLOG_WARN("Failed to report, hiview is crashed.");
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
    info.faultLogType = 2; // 2 : CPP_CRASH_TYPE
    info.module = cmdline_;
    info.reason = reason_;
    info.summary = stack_;
    info.registers = registers_;
    info.otherThreadInfo = otherThreadInfo_;
    info.sectionMaps = kvPairs_;
    addFaultLog(&info);
    DFXLOG_INFO("Finish report fault to FaultLogger %s(%d,%d)", cmdline_.c_str(), pid_, uid_);
    dlclose(handle);
}

void CppCrashReporter::ReportToAbilityManagerService()
{
    if (process_->processInfo_.processName.find(FOUNDATION_PROCESS_NAME) != std::string::npos) {
        DFXLOG_WARN("Do not to report to AbilityManagerService, foundation is crashed.");
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
    recordAppExitReason(cppCrashExitReason);
    dlclose(handle);
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
