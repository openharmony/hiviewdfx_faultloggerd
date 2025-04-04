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

#ifndef CPP_CRASH_REPORTER_H
#define CPP_CRASH_REPORTER_H

#include <memory>
#include <string>
#include "dfx_dump_request.h"
#include "dfx_process.h"

namespace OHOS {
namespace HiviewDFX {
class CppCrashReporter {
public:
    explicit CppCrashReporter(uint64_t time) : time_(time){};

    void SetCrashReason(const std::string& reason)
    {
        reason_ = reason;
    };

    void AppendCrashStack(const std::string& frame)
    {
        stack_.append(frame).append("\n");
    };

    void SetCppCrashInfo(const std::string& cppCrashInfo)
    {
        cppCrashInfo_ = cppCrashInfo;
    };
    void ReportToHiview(const DfxProcess& process);
    void ReportToAbilityManagerService(const DfxProcess& process);

private:
    bool Format(const DfxProcess& process);
    static std::string GetRegsString(std::shared_ptr<DfxRegs> regs);
    int32_t WriteCppCrashInfoByPipe();

    uint64_t time_;
    int32_t pid_{0};
    uint32_t uid_{0};
    std::string cmdline_;
    std::string reason_;
    std::string stack_;
    std::string registers_;
    std::string cppCrashInfo_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
