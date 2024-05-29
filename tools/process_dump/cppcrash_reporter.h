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

#include <cinttypes>
#include <csignal>
#include <map>
#include <memory>
#include <string>
#include "dfx_dump_request.h"
#include "dfx_process.h"

namespace OHOS {
namespace HiviewDFX {
class CppCrashReporter {
public:
    CppCrashReporter(uint64_t time, std::shared_ptr<DfxProcess> process, int32_t dumpMode) \
        : time_(time), dumpMode_(dumpMode), process_(process)
    {
        pid_ = 0;
        uid_ = 0;
        cmdline_ = "";
        reason_ = "";
        stack_ = "";
    };
    virtual ~CppCrashReporter() {};

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
    void ReportToHiview();
    void ReportToAbilityManagerService();

private:
    bool Format();
    static std::string GetRegsString(std::shared_ptr<DfxRegs> regs);
    int32_t WriteCppCrashInfoByPipe();

private:
    uint64_t time_;
    int32_t pid_;
    uint32_t uid_;
    int32_t dumpMode_ = FUSION_MODE;
    std::string cmdline_;
    std::string reason_;
    std::string stack_;
    std::string registers_ = "";
    std::string cppCrashInfo_ = "";
    std::shared_ptr<DfxProcess> process_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
