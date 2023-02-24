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

#ifndef RUSTPANIC_LISTENER_H
#define RUSTPANIC_LISTENER_H

#include "hisysevent_listener.h"

#include <unordered_set>

namespace OHOS {
namespace HiviewDFX {
class RustPanicListener : public HiSysEventListener {
public:
    explicit RustPanicListener() : HiSysEventListener() {}
    virtual ~RustPanicListener() {}

public:
    void OnEvent(std::shared_ptr<HiSysEventRecord> sysEvent);
    void OnServiceDied();

public:
    bool CheckKeywordInReasons(const std::string& reason);

private:
    std::unordered_set<std::string> reasons;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif