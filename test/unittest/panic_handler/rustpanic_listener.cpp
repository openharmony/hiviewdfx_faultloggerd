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

namespace OHOS {
namespace HiviewDFX {
void RustPanicListener::OnEvent(std::shared_ptr<HiSysEventRecord> sysEvent)
{
    if (sysEvent == nullptr) {
        return;
    }
    std::string reason;
    sysEvent->GetParamValue("REASON", reason);
    reasons.emplace(reason);
}

void RustPanicListener::OnServiceDied()
{
    std::cout << std::string("service disconnected, exit.") << std::endl;
}

bool RustPanicListener::CheckKeywordInReasons(const std::string& reason)
{
    for (auto& rs : reasons) {
        if (rs.find(reason) != std::string::npos) {
            return true;
        }
    }
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS