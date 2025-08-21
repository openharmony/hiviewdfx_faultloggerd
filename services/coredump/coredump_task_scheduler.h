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
#ifndef COREDUMP_TASK_SCHEDULER_H
#define COREDUMP_TASK_SCHEDULER_H

#include "coredump_model.h"
#include "coredump_session_manager.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpTaskScheduler {
public:
    CoredumpTaskScheduler() : sessionManager_(CoredumpSessionManager::Instance()) {}
    void ScheduleCancelTime(SessionId sessionId, int timeoutMs);
    void CancelTimeout(SessionId sessionId);
private:
    CoredumpSessionManager& sessionManager_;
};
}
}
#endif
