/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "crash_exception_listener.h"

#include <cstring>
#include <iostream>

namespace OHOS {
namespace HiviewDFX {
void CrashExceptionListener::SetKeyWord(const std::string& keyWord)
{
    this->keyWord = keyWord;
    std::lock_guard<std::mutex> lock(setFlagMutex);
    allFindFlag = false;
}

void CrashExceptionListener::OnEvent(std::shared_ptr<HiSysEventRecord> sysEvent)
{
    if (sysEvent == nullptr) {
        return;
    }
    std::string reason;
    sysEvent->GetParamValue("PROCESS_NAME", reason);

    if (reason.find(keyWord) == std::string::npos) {
        return;
    }

    // find all keywords, set allFindFlag to true
    std::lock_guard<std::mutex> lock(setFlagMutex);
    allFindFlag = true;
    keyWordCheckCondition.notify_all();
}

void CrashExceptionListener::OnServiceDied()
{
}

bool CrashExceptionListener::CheckKeywordInReasons()
{
    std::unique_lock<std::mutex> lock(setFlagMutex);
    if (allFindFlag) {
        return true;
    }

    auto flagCheckFunc = [this]() {
        return this->allFindFlag;
    };

    // wait allFindFlag set true for 6 seconds
    if (keyWordCheckCondition.wait_for(lock, std::chrono::seconds(6), flagCheckFunc)) {
        return true;
    } else {
        return false;
    }
}
} // namespace HiviewDFX
} // namespace OHOS