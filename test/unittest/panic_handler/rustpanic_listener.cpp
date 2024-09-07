/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "rustpanic_listener.h"

#include <cstring>
#include <iostream>
#ifdef HISYSEVENT_ENABLE
namespace OHOS {
namespace HiviewDFX {
void RustPanicListener::SetKeyWord(const std::string& keyWord)
{
    std::cout << "Enter SetKeyWord:" << keyWord << std::endl;
    this->keyWord = keyWord;
    {
        std::lock_guard<std::mutex> lock(setFlagMutex);
        allFindFlag = false;
    }
}

void RustPanicListener::OnEvent(std::shared_ptr<HiSysEventRecord> sysEvent)
{
    if (sysEvent == nullptr) {
        return;
    }
    std::string reason;
    sysEvent->GetParamValue("REASON", reason);

    std::cout << "recv event, reason:" << reason << std::endl;
    if (reason.find(keyWord) == std::string::npos) {
        std::cout << "not find keyWord in reason"  << std::endl;
        return;
    }

    // find all keywords, set allFindFlag to true
    std::cout << "OnEvent get keyWord"  << std::endl;
    {
        std::lock_guard<std::mutex> lock(setFlagMutex);
        allFindFlag = true;
        keyWordCheckCondition.notify_all();
    }
}

void RustPanicListener::OnServiceDied()
{
    std::cout << std::string("service disconnected, exit.") << std::endl;
}

bool RustPanicListener::CheckKeywordInReasons()
{
    std::cout << "Enter CheckKeywordInReasons"  << std::endl;
    std::unique_lock<std::mutex> lock(setFlagMutex);
    if (allFindFlag) {
        std::cout << "allFindFlag is true, match ok, return true"  << std::endl;
        return true;
    }

    auto flagCheckFunc = [&]() {
        return allFindFlag;
    };

    // 6: wait allFindFlag set true for 6 seconds
    if (keyWordCheckCondition.wait_for(lock, std::chrono::seconds(6), flagCheckFunc)) {
        std::cout << "match ok, return true"  << std::endl;
        return true;
    } else {
        std::cout << "match keywords timeout"  << std::endl;
        return false;
    }
}
} // namespace HiviewDFX
} // namespace OHOS
#endif