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

#ifndef TIMEOUT_ANALYZER_H
#define TIMEOUT_ANALYZER_H

#include <cstdint>
#include <string>

namespace OHOS {
namespace HiviewDFX {

class TimeoutAnalyzer {
public:
    void AnalyzeTimeoutReason(int pid, int32_t& ret);
    bool IsFrozen(int pid, int32_t& ret);

private:
    bool IsSignalBlocked(int pid, int32_t& ret);
    bool IsBitOn(const std::string& content, const std::string& field, int signal);
    bool FindWatchdogTid(int pid, int& targetTid);
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // TIMEOUT_ANALYZER_H
