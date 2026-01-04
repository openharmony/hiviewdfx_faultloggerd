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

#include "elapsed_time.h"

#include <cinttypes>

#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
ElapsedTime::~ElapsedTime()
{
    if (limitCostMilliseconds_ != 0) {
        time_t costTime = Elapsed<std::chrono::milliseconds>();
        if (costTime > limitCostMilliseconds_) {
            DFXLOGW("%{public}s running %{public}" PRId64 " ms", printContent_.c_str(), costTime);
        }
    }
}

void ElapsedTime::Reset()
{
    begin_ = std::chrono::high_resolution_clock::now();
}
}
}
