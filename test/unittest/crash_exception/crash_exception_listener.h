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

#ifndef CRASHEXCEPTION_LISTENER_H
#define CRASHEXCEPTION_LISTENER_H

#include "hisysevent_listener.h"

namespace OHOS {
namespace HiviewDFX {
class CrashExceptionListener : public HiSysEventListener {
public:
    explicit CrashExceptionListener() : HiSysEventListener() {}
    virtual ~CrashExceptionListener() {}

public:
    void SetKeyWord(const std::string& keyWord);
    bool CheckKeywordInReasons();
    void OnEvent(std::shared_ptr<HiSysEventRecord> sysEvent);
    void OnServiceDied();

private:
    std::string keyWord;
    bool allFindFlag = false;
    std::mutex setFlagMutex;
    std::condition_variable keyWordCheckCondition;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif