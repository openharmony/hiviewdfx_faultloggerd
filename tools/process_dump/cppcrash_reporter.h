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
#include "dfx_process.h"

namespace OHOS {
namespace HiviewDFX {
class CppCrashReporter {
public:
    CppCrashReporter(uint64_t time, std::shared_ptr<DfxProcess> process) \
        : time_(time), process_(process)
    {
        pid_ = 0;
        uid_ = 0;
        cmdline_ = "";
        reason_ = "";
        stack_ = "";
        registers_ = "";
        otherThreadInfo_ = "";
    };
    virtual ~CppCrashReporter() {};

    void SetCrashReason(const std::string& reason)
    {
        reason_ = reason;
    };

    void AppendCrashStack(const std::string& frame) {
        stack_.append(frame).append("\n");
    };

    void SetValue(const std::string& key, const std::string& value)
    {
        if (key.empty() || value.empty()) {
            return;
        }

        kvPairs_[key] = value;
    };
    void ReportToHiview();
    void ReportToAbilityManagerService();

private:
    bool Format();
    std::string GetRegsString(std::shared_ptr<DfxThread> thread) const;

private:
    uint64_t time_;
    int32_t pid_;
    uint32_t uid_;
    std::string cmdline_;
    std::string reason_;
    std::string stack_;
    std::string registers_;
    std::string otherThreadInfo_;
    std::map<std::string, std::string> kvPairs_;
    std::shared_ptr<DfxProcess> process_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
