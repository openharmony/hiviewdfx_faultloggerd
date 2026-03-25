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

#ifndef REPORTER_H
#define REPORTER_H

#include <memory>
#include <string>
#include "dfx_dump_request.h"
#include "dfx_process.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
class Reporter {
public:
    virtual ~Reporter() = default;
    virtual void Report(DfxProcess& process, const ProcessDumpRequest &request) = 0;
};

class SysEventReporter {
public:
    explicit SysEventReporter(const ProcessDumpType& processDumpType);
    void Report(DfxProcess& process, const ProcessDumpRequest &request);
private:
    std::shared_ptr<Reporter> reporter_ = nullptr;
};

class CppCrashReporter : public Reporter {
public:
    void Report(DfxProcess& process, const ProcessDumpRequest &request);
private:
    void ReportToHiview(DfxProcess& process, const ProcessDumpRequest &request);
    std::string GetRegsString(std::shared_ptr<DfxRegs> regs);
    SmartFd TranferCrashInfoToHiview(const std::string& cppCrashInfo);
    std::string GetSummary(DfxProcess& process);
};

class AddrSanitizerReporter : public Reporter {
public:
    void Report(DfxProcess& process, const ProcessDumpRequest &request);
};

void ReportToAbilityManagerService(const DfxProcess& process, const ProcessDumpRequest &request);
} // namespace HiviewDFX
} // namespace OHOS
#endif
