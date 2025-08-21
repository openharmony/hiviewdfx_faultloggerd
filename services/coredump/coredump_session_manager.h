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
#ifndef COREDUMP_SESSION_MANAGER_H
#define COREDUMP_SESSION_MANAGER_H

#include <unordered_map>

#include "coredump_model.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpSessionManager {
public:
    static CoredumpSessionManager& Instance();
    SessionId CreateSession(const CreateCoredumpRequest& request);
    CoredumpSession* GetSession(SessionId sessionId);
    void RemoveSession(SessionId sessionId);

    CoredumpSessionManager(const CoredumpSessionManager&) = delete;
    CoredumpSessionManager& operator =(const CoredumpSessionManager&) = delete;
private:
    CoredumpSessionManager() = default;
    std::unordered_map<SessionId, CoredumpSession> sessions_;
};
}
}
#endif
