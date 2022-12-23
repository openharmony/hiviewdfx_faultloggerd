/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files is writer log to file module on process dump module. */

#include "cppcrash_reporter.h"

#include <cinttypes>
#include <dlfcn.h>
#include <map>
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
    std::map<std::string, std::string> sectionMaps;
};
static const char HIVIEW_PROCESS_NAME[] = "/system/bin/hiview";

using AddFaultLog = void (*)(FaultLogInfoInner* info);
namespace OHOS {
namespace HiviewDFX {

bool CppCrashReporter::Format()
{
    if (process_ == nullptr) {
        return false;
    }

    cmdline_ = process_->GetProcessName();
    pid_ = process_->GetPid();
    uid_ = process_->GetUid();
    reason_ = FormatSignalName(signo_);
    auto threads = process_->GetThreads();
    std::shared_ptr<DfxThread> crashThread = nullptr;
    if (!threads.empty()) {
        crashThread = threads.front();
    }

    if (crashThread != nullptr) {
        stack_ = crashThread->ToString();
    }
    return true;
}

void CppCrashReporter::ReportToHiview()
{
    if (!Format()) {
        DfxLogWarn("Failed to format crash report.");
        return;
    }
    if (process_->GetProcessName().find(HIVIEW_PROCESS_NAME) != std::string::npos) {
        DfxLogWarn("Failed to report, hiview is crashed.");
        return;
    }

    void* handle = dlopen("libfaultlogger.z.so", RTLD_LAZY | RTLD_NODELETE);
    if (handle == nullptr) {
        DfxLogWarn("Failed to dlopen libfaultlogger, %s\n", dlerror());
        return;
    }

    AddFaultLog addFaultLog = (AddFaultLog)dlsym(handle, "AddFaultLog");
    if (addFaultLog == nullptr) {
        DfxLogWarn("Failed to dlsym AddFaultLog, %s\n", dlerror());
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
    info.sectionMaps = kvPairs_;
    addFaultLog(&info);
    DfxLogInfo("Finish report fault to FaultLogger %s(%d,%d)", cmdline_.c_str(), pid_, uid_);
    dlclose(handle);
}
} // namespace HiviewDFX
} // namespace OHOS
